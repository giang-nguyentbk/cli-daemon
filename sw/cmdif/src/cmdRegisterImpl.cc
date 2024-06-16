#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <functional>
#include <memory>

#include <itc.h>
#include <itcPubSubIf.h>
#include <traceIf.h>

#include "cmdRegisterImpl.h"
#include "cmdJobImpl.h"
#include "cmdTypesIf.h"
#include "cmdProto.h"

namespace // anonymous namespace
{

using namespace CmdIf::V1;



} // anonymous namespace

namespace CmdIf
{

namespace V1
{

using namespace UtilsFramework::ItcPubSub::V1;

CmdRegisterIf& CmdRegisterIf::getInstance()
{
	return CmdRegisterImpl::getInstance();
}

CmdRegisterImpl& CmdRegisterImpl::getInstance()
{
	static CmdRegisterImpl instance;
	return instance;
}

CmdRegisterImpl::CmdRegisterImpl()
{
	init();
}

void CmdRegisterImpl::reset()
{
	m_registeredInvokers.clear();
}

CmdRegisterIf::ReturnCode CmdRegisterImpl::registerCmdHandler(const std::string& cmdName, const std::string& cmdDesc, const CmdCallback& cmdHandler)
{
	CmdRegisterIf::ReturnCode rc = CmdRegisterIf::ReturnCode::ALREADY_EXISTS;
	std::unique_lock<std::mutex> lock(m_mutex);

	auto iter = m_registeredInvokers.find(cmdName);
	if(iter == m_registeredInvokers.end())
	{
		m_registeredInvokers.emplace(cmdName, std::bind(&CmdRegisterImpl::invokeCmd, this, std::placeholders::_1, cmdHandler));
		lock.unlock();

		union itc_msg* req = itc_alloc(offsetof(struct CmdIfRegCmdRequestS, cmd_desc) + cmdDesc.length() + 1, CMDIF_REG_CMD_REQUEST);

		if(cmdName.length() < MAX_CMD_NAME_LENGTH)
		{
			std::memcpy(req->cmdIfRegCmdRequest.cmd_name, cmdName.c_str(), cmdName.length() + 1);
		} else
		{
			// Reserve 1 byte for '\0'
			std::memcpy(req->cmdIfRegCmdRequest.cmd_name, cmdName.substr(0, MAX_CMD_NAME_LENGTH - 1).c_str(), MAX_CMD_NAME_LENGTH);
		}

		std::memcpy(req->cmdIfRegCmdRequest.cmd_desc, cmdDesc.c_str(), cmdDesc.length() + 1);

		if(m_clidMboxId == ITC_NO_MBOX_ID)
		{
			rc = CmdRegisterIf::ReturnCode::INTERNAL_ERROR;
			return rc;
		}

		if(!itc_send(&req, m_clidMboxId, ITC_MY_MBOX_ID, NULL))
		{
			TPT_TRACE(TRACE_ERROR, "Failed to send CMDIF_REG_CMD_REQUEST to clid for cmdName = %s!", cmdName);
			rc = CmdRegisterIf::ReturnCode::INTERNAL_ERROR;
			return rc;
		}

		TPT_TRACE(TRACE_INFO, "Send CMDIF_REG_CMD_REQUEST to clid successfully!");

		rc = CmdRegisterIf::ReturnCode::NORMAL;
	}

	TPT_TRACE(TRACE_ABN, "Command name %s already exists!", cmdName);
	return rc;
}

CmdRegisterIf::ReturnCode CmdRegisterImpl::deregisterCmdHandler(const std::string& cmdName)
{
	CmdRegisterIf::ReturnCode rc = CmdRegisterIf::ReturnCode::NOT_FOUND;
	std::unique_lock<std::mutex> lock(m_mutex);

	auto iter = m_registeredInvokers.find(cmdName);
	if(iter != m_registeredInvokers.end())
	{
		m_registeredInvokers.erase(cmdName);
		lock.unlock();

		union itc_msg* req = itc_alloc(offsetof(struct CmdIfDeregCmdRequestS, cmd_name), CMDIF_DEREG_CMD_REQUEST);
		
		if(cmdName.length() < MAX_CMD_NAME_LENGTH)
		{
			std::memcpy(req->cmdIfDeregCmdRequest.cmd_name, cmdName.c_str(), cmdName.length() + 1);
		} else
		{
			// Reserve 1 byte for '\0'
			std::memcpy(req->cmdIfDeregCmdRequest.cmd_name, cmdName.substr(0, MAX_CMD_NAME_LENGTH - 1).c_str(), MAX_CMD_NAME_LENGTH);
		}

		if(m_clidMboxId == ITC_NO_MBOX_ID)
		{
			rc = CmdRegisterIf::ReturnCode::INTERNAL_ERROR;
			return rc;
		}

		if(!itc_send(&req, m_clidMboxId, ITC_MY_MBOX_ID, NULL))
		{
			TPT_TRACE(TRACE_ERROR, "Failed to send CMDIF_DEREG_CMD_REQUEST to clid for cmdName = %s!", cmdName);
			rc = CmdRegisterIf::ReturnCode::INTERNAL_ERROR;
			return rc;
		}

		TPT_TRACE(TRACE_INFO, "Send CMDIF_DEREG_CMD_REQUEST to clid successfully!");

		rc = CmdRegisterIf::ReturnCode::NORMAL;
	}

	TPT_TRACE(TRACE_ABN, "Command name %s not found!", cmdName);
	return rc;
}

void invokeCmd(const std::shared_ptr<CmdIf::V1::CmdJobIf>& job, const CmdRegisterIf::CmdCallback& cmdHandler)
{
	TPT_TRACE(TRACE_INFO, "Invoking cmd handler: %s", job->getCmdName());
	cmdHandler(job);
}

void CmdRegisterImpl::init()
{
	TPT_TRACE(TRACE_INFO, "CmdRegisterIf initializing...");

	m_clidMboxId = itc_locate_sync(ITC_NO_WAIT, m_clidMboxName.c_str(), true, NULL, NULL);
	if(m_clidMboxId == ITC_NO_MBOX_ID)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to locate %s!", m_clidMboxName);
		return;
	}

	IItcPubSub& itcPubSub = IItcPubSub::getInstance();
	itcPubSub.registerMsg(CMDIF_EXE_CMD_REQUEST, std::bind(&CmdRegisterImpl::handleExeCmdRequest, this, std::placeholders::_1));
}

void CmdRegisterImpl::handleExeCmdRequest(const std::shared_ptr<union itc_msg>& msg)
{
	// TODO: Create a Job, save job_id and pass it to the class who registered the cmdHandler.
	// Reinterpret cmd arguments sent from clid daemon and pass to cmdTableIf for decoding the syntax and executing the actual cmd

	std::string printArgs {"\""};
	std::vector<std::string> argsList;
	uint32_t numArgs = msg->cmdIfExeCmdRequest.num_args;
	char* args = msg->cmdIfExeCmdRequest.payload;

	for(uint32_t i = 0; i < numArgs; i++)
	{
		std::string str(args);
		printArgs += str;
		printArgs += " ";

		argsList.push_back(str);
		args += (str.length() + 1);
		
	}

	printArgs += "\"";
	TPT_TRACE(TRACE_INFO, "Received execute command request from clid for cmdName %s, with args %s", msg->cmdIfExeCmdRequest.cmd_name, printArgs.c_str());

	auto job = std::make_shared<CmdIf::V1::CmdJobImpl>(msg->cmdIfExeCmdRequest.cmd_name, msg->cmdIfExeCmdRequest.job_id, argsList, m_clidMboxId);

	auto iter = m_registeredInvokers.find(job->getCmdName());
	if(iter != m_registeredInvokers.cend())
	{
		// Pass the job to registered cmdHandler which previously added by registerCmdHandler()
		iter->second(job);
	} else
	{
		TPT_TRACE(TRACE_ERROR, "No registered cmdHandler found for cmdName %s", job->getCmdName());
	}
}

} // V1

} // namespace CmdIf