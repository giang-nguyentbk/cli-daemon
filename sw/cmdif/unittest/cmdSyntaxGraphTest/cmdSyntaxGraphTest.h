/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#pragma once

#include "cmdSyntaxGraph.h"


namespace CmdIf
{

namespace V1
{

class CmdSyntaxGraphTest
{
public:
	CmdSyntaxGraphTest() = default;
	virtual ~CmdSyntaxGraphTest() = default;

	bool getNextTokenTest(const char*& syntax, std::string& token);

private:
	CmdSyntaxGraph m_syntaxGraph;

}; // class CmdSyntaxGraphTest

	
} // namespace V1

} // namespace CmdIf