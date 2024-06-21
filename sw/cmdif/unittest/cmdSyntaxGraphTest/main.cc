#include <iostream>
#include <string>
#include <functional>
#include <sstream>

#include "cmdSyntaxGraphTest.h"


using namespace CmdIf::V1;

int mockCmdHandler3(int a, int b)
{
	(void)a;
	(void)b;
	std::cout << "cmdHandler3 get called!" << std::endl;
	return a;
}

int main()
{
	CmdSyntaxGraphTest syntaxGraphTest;

	std::vector<std::string> arguments {"abc", "def"};
	std::ostringstream outputStream;

	// const char* syntax1 = "abc { def | ghi } jkl";
	const char* syntax2 = "abc [ def | ghi <jkl> | ghi <jkl> | { mno | pqr } ]";
	// const char* syntax3 = "abc { def | ghi jkl";
	const char* syntax4 = "abc [ vwy ] def";

	std::shared_ptr<GraphNode> firstNode = std::make_shared<GraphNode>("abc");

	CmdTypesIf::CmdFunctionWrapper cmdHandler1;
	cmdHandler1.func = std::bind(&CmdSyntaxGraphTest::mockCmdHandler, &syntaxGraphTest, std::placeholders::_1, std::placeholders::_2);
	cmdHandler1.funcName = "CmdIf::V1::CmdSyntaxGraphTest::mockCmdHandler";

	CmdTypesIf::CmdFunctionWrapper cmdHandler2;
	cmdHandler2.func = std::bind(&CmdSyntaxGraphTest::mockCmdHandler2, &syntaxGraphTest, std::placeholders::_1, std::placeholders::_2);
	cmdHandler2.funcName = "CmdIf::V1::CmdSyntaxGraphTest::mockCmdHandler2";

	// syntaxGraphTest.getNextTokenTest(syntax1);
	// syntaxGraphTest.getNextTokenTest(syntax2);
	// syntaxGraphTest.getNextTokenTest(syntax3);

	// syntaxGraphTest.splitOutSyntaxTest(syntax1);
	// syntaxGraphTest.splitOutSyntaxTest(syntax2);
	// syntaxGraphTest.splitOutSyntaxTest(syntax3);

	syntaxGraphTest.addSyntax(firstNode, syntax2, cmdHandler1);
	(firstNode->m_handler.func)(arguments, outputStream);

	syntaxGraphTest.addSyntax(firstNode, syntax4, cmdHandler2);
	(firstNode->m_handler.func)(arguments, outputStream);

	syntaxGraphTest.evaluateCommandArguments(firstNode, arguments);

	syntaxGraphTest.printNextPossibleArgumentsTest(firstNode);

	return 0;
}