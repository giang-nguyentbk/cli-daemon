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

namespace CmdIf
{

namespace V1
{

class CmdTableIf
{
public:
	static CmdTableIf& getInstance();

	

	

	virtual void registerCmdTable(const std::string& cmdName, const std::vector<CmdTypesIf::CmdDefinition>& cmdDefinitions) = 0;
	virtual CmdIf::V1::CmdTypesIf::CmdResultCode executeCmd(const std::vector<std::string>& args, std::string& output) = 0;
	virtual void printCmdHelp(const std::vector<CmdTypesIf::CmdDefinition>& cmdDefinitions, std::string& output) = 0;

	// Avoid copy/move constructors, assigments
	CmdTableIf(const CmdTableIf&) 			= delete;
	CmdTableIf(CmdTableIf&&) 			= delete;
	CmdTableIf& operator=(const CmdTableIf&) 	= delete;
	CmdTableIf& operator=(CmdTableIf&&) 		= delete;

protected:
	CmdTableIf() = default;
	~CmdTableIf() = default;

}; // class CmdTableIf

} // V1

} // namespace CmdIf