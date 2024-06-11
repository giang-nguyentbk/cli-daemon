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
#include "cmdJobIf.h"
#include "cmdTypesIf.h"
#include "cmdProto.h"


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

CmdRegisterIf::ReturnCode CmdRegisterImpl::registerCmdHandler(const std::string& cmdName, const std::string& cmdDesc, const CmdHandler& cmdHandler)
{
	CmdRegisterIf::ReturnCode rc = CmdRegisterIf::ReturnCode::ALREADY_EXISTS;
	std::unique_lock<std::mutex> lock(m_mutex);

	auto iter = m_registeredInvokers.find(cmdName);
	if(iter == m_registeredInvokers.end())
	{
		m_registeredInvokers.emplace(cmdName, std::bind(&invokeCmd, std::placeholders::_1, cmdHandler));
		lock.unlock();

		union itc_msg* req = itc_alloc(offsetof(struct CmdIfRegCmdRequestS, cmd_desc) + cmdDesc.length() + 1, CMDIF_REG_CMD_REQUEST);
		req->cmdIfRegCmdRequest.mailbox_id = itc_current_mbox();

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
			return;
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
			return;
		}

		TPT_TRACE(TRACE_INFO, "Send CMDIF_DEREG_CMD_REQUEST to clid successfully!");

		rc = CmdRegisterIf::ReturnCode::NORMAL;
	}

	TPT_TRACE(TRACE_ABN, "Command name %s not found!", cmdName);
	return rc;
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
	itcPubSub.registerMsg(CMDIF_EXE_CMD_REQUEST, std::bind(&handleExeCmdRequest, this, std::placeholders::_1));
}

void CmdRegisterImpl::invokeCmd(const std::shared_ptr<CmdIf::V1::CmdJobIf>& job, const CmdRegisterIf::CmdHandler& cmdHandler)
{
	TPT_TRACE(TRACE_INFO, "Invoking cmd handler: %s", job->getCmdName());
	cmdHandler(job);
}

void CmdRegisterImpl::handleExeCmdRequest(const std::shared_ptr<union itc_msg>& msg)
{

}

} // V1

} // namespace CmdIf