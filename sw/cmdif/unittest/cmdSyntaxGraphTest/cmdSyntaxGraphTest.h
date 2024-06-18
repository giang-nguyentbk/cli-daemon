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

	void getNextTokenTest(const char* syntax);
	void splitOutSyntaxTest(const char* syntax);
	std::shared_ptr<GraphNode> addSyntax(std::shared_ptr<GraphNode> firstNode, const char* syntax, const CmdTypesIf::CmdFunctionWrapper& cmdHandler);
	void evaluateCommandArguments(std::shared_ptr<GraphNode> firstNode, const std::vector<std::string>& args);

	CmdTypesIf::CmdResultCode mockCmdHandler(const std::vector<std::string>& arguments, std::string& outputStream);
	CmdTypesIf::CmdResultCode mockCmdHandler2(const std::vector<std::string>& arguments, std::string& outputStream);
	int mockCmdHandler4(int a, int b);

private:
	CmdSyntaxGraph m_syntaxGraph;

}; // class CmdSyntaxGraphTest

	
} // namespace V1

} // namespace CmdIf