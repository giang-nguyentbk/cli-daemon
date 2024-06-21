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
#include <vector>
#include <string>

#include "cmdTypesIf.h"
#include "cmdJobIf.h"

namespace CmdIf
{

namespace V1
{

class CmdRegisterIf
{
public:
	enum class ReturnCode
	{
		NORMAL,
		ALREADY_EXISTS,
		NOT_FOUND,
		INTERNAL_ERROR
	};

	static CmdRegisterIf& getInstance();

	using CmdInvoker = std::function<void(const std::shared_ptr<CmdIf::V1::CmdJobIf>& job)>;

	virtual ReturnCode registerCmdHandler(const std::string& cmdName, const std::string& cmdDesc, const CmdInvoker& cmdHandler) = 0;
	virtual ReturnCode deregisterCmdHandler(const std::string& cmdName) = 0;
	
	// Avoid copy/move constructors, assigments
	CmdRegisterIf(const CmdRegisterIf&) 		= delete;
	CmdRegisterIf(CmdRegisterIf&&) 			= delete;
	CmdRegisterIf& operator=(const CmdRegisterIf&) 	= delete;
	CmdRegisterIf& operator=(CmdRegisterIf&&) 	= delete;

protected:
	CmdRegisterIf() = default;
	~CmdRegisterIf() = default;

}; // class CmdRegisterIf

} // V1

} // namespace CmdIf