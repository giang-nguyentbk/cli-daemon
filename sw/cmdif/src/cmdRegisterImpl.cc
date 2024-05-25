#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <functional>
#include <memory>

#include <itc.h>
#include <itcPubSubIf.h>

#include "cmdRegisterImpl.h"
#include "cmdJobIf.h"
#include "cmdTypesIf.h"
#include "cmdProto.h"
#include "traceUtils.h"

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

CmdRegisterIf::ReturnCode CmdRegisterImpl::registerCmdHandler(const std::string& cmdName, const CmdHandler& cmdHandler)
{
	CmdRegisterIf::ReturnCode rc = CmdRegisterIf::ReturnCode::ALREADY_EXISTS;
	std::unique_lock<std::mutex> lock(m_mutex);

	if(m_registeredInvokers.count(cmdName) == 0)
	{
		m_registeredInvokers.emplace(cmdName, std::bind(&invokeCmd, std::placeholders::_1, cmdHandler));
		lock.unlock();

		union itc_msg* req = itc_alloc(offsetof(struct CmdIfRegCmdRequestS, payload), CMDIF_REG_CMD_REQUEST);
		req->cmdIfRegCmdRequest.payloadLen = cmdName.length();
		std::memcpy(req->cmdIfRegCmdRequest.payload, cmdName.c_str(), cmdName.length());

		if(m_clidMboxId == ITC_NO_MBOX_ID)
		{
			rc = CmdRegisterIf::ReturnCode::INTERNAL_ERROR;
			return rc;
		}

		if(!itc_send(&req, m_clidMboxId, ITC_MY_MBOX_ID, NULL))
		{
			LOG_ERROR("Failed to send CMDIF_REG_CMD_REQUEST to clid for cmdName = %s!\n", cmdName);
			return;
		}

		LOG_INFO("Send CMDIF_REG_CMD_REQUEST to clid successfully!\n");

		rc = CmdRegisterIf::ReturnCode::NORMAL;
	}

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

		union itc_msg* req = itc_alloc(offsetof(struct CmdIfDeregCmdRequestS, payload), CMDIF_DEREG_CMD_REQUEST);
		req->cmdIfDeregCmdRequest.payloadLen = cmdName.length();
		std::memcpy(req->cmdIfDeregCmdRequest.payload, cmdName.c_str(), cmdName.length());

		if(m_clidMboxId == ITC_NO_MBOX_ID)
		{
			rc = CmdRegisterIf::ReturnCode::INTERNAL_ERROR;
			return rc;
		}

		if(!itc_send(&req, m_clidMboxId, ITC_MY_MBOX_ID, NULL))
		{
			LOG_ERROR("Failed to send CMDIF_DEREG_CMD_REQUEST to clid for cmdName = %s!\n", cmdName);
			return;
		}

		LOG_INFO("Send CMDIF_DEREG_CMD_REQUEST to clid successfully!\n");

		rc = CmdRegisterIf::ReturnCode::NORMAL;
	}

	return rc;
}

void CmdRegisterImpl::init()
{
	LOG_INFO("CmdRegisterIf initializing...\n");

	m_clidMboxId = itc_locate_sync(1000, m_clidMboxName.c_str(), true, NULL, NULL);
	if(m_clidMboxId == ITC_NO_MBOX_ID)
	{
		LOG_ERROR("Failed to locate %s even after 1000ms!\n", m_clidMboxName);
		return;
	}

	IItcPubSub& itcPubSub = IItcPubSub::getInstance();
	itcPubSub.registerMsg(CMDIF_REG_CMD_REPLY, std::bind(&handleRegCmdListReply, this, std::placeholders::_1));
	itcPubSub.registerMsg(CMDIF_DEREG_CMD_REPLY, std::bind(&handleDeregCmdListReply, this, std::placeholders::_1));
	itcPubSub.registerMsg(CMDIF_EXE_CMD_REQUEST, std::bind(&handleExeCmdRequest, this, std::placeholders::_1));
}

void CmdRegisterImpl::invokeCmd(const std::shared_ptr<CmdIf::V1::CmdJobIf>& job, const CmdRegisterIf::CmdHandler& cmdHandler)
{
	LOG_INFO("Invoking cmd handler: %s\n", job->getCmdName());
	cmdHandler(job);
}

void CmdRegisterImpl::handleRegCmdListReply(const std::shared_ptr<union itc_msg>& msg)
{

}

void CmdRegisterImpl::handleDeregCmdListReply(const std::shared_ptr<union itc_msg>& msg)
{

}

void CmdRegisterImpl::handleExeCmdRequest(const std::shared_ptr<union itc_msg>& msg)
{

}

} // V1

} // namespace CmdIf