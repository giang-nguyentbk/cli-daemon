/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#pragma once

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

}; // class CmdRegisterIf

} // V1

} // namespace CmdIf