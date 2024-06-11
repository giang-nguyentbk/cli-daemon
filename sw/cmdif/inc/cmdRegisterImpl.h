/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>

#include <itc.h>

#include "cmdRegisterIf.h"
#include "cmdTypesIf.h"
#include "cmdJobIf.h"

namespace CmdIf
{

namespace V1
{

class CmdRegisterImpl : public CmdRegisterIf
{
public:
	static CmdRegisterImpl& getInstance();
	
	void reset();

	CmdRegisterImpl();
	virtual ~CmdRegisterImpl() = default;

	// Avoid copy/move constructors, assigments
	CmdRegisterImpl(const CmdRegisterImpl&) 		= delete;
	CmdRegisterImpl(CmdRegisterImpl&&) 			= delete;
	CmdRegisterImpl& operator=(const CmdRegisterImpl&) 	= delete;
	CmdRegisterImpl& operator=(CmdRegisterImpl&&) 		= delete;

	ReturnCode registerCmdHandler(const std::string& cmdName, const std::string& cmdDesc, const CmdHandler& cmdHandler) override;
	ReturnCode deregisterCmdHandler(const std::string& cmdName) override;

private:
	void init();
	using CmdInvoker = std::function<void(const std::shared_ptr<CmdIf::V1::CmdJobIf>& job)>;
	void invokeCmd(const std::shared_ptr<CmdIf::V1::CmdJobIf>& job, const CmdRegisterIf::CmdHandler& cmdHandler);

	void handleRegCmdListReply(const std::shared_ptr<union itc_msg>& msg);
	void handleDeregCmdListReply(const std::shared_ptr<union itc_msg>& msg);
	void handleExeCmdRequest(const std::shared_ptr<union itc_msg>& msg);

private:
	const std::string m_clidMboxName {"clidMailbox"};
	std::mutex m_mutex;
	itc_mbox_id_t m_clidMboxId;
	std::unordered_map<std::string, CmdInvoker> m_registeredInvokers;

}; // class CmdRegisterImpl

} // V1

} // namespace CmdIf