/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#pragma once

#include <string>
#include <vector>
#include <functional>

namespace CmdIf
{

namespace V1
{
class CmdTypesIf
{
public:
	enum class CmdResultCode
	{
		CMD_RET_SUCCESS,
		CMD_RET_INVALID_ARGS,
		CMD_RET_FAIL
	};

	using CmdFunction = std::function<CmdResultCode(const std::vector<std::string>& arguments, std::ostringstream& outputStream)>;

	struct CmdFunctionWrapper
	{
		CmdTypesIf::CmdFunction func;
		std::string funcName;
	};

	struct CmdDefinition
	{
		std::string m_syntax;
		CmdFunctionWrapper m_handler;
		std::string m_desc;
	};

}; // class CmdTypesIf

} // V1

} // namespace CmdIf