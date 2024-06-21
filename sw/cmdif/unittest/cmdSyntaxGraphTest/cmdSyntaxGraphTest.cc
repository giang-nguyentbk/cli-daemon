#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <string>

#include "cmdSyntaxGraphTest.h"
#include "cmdTypesIf.h"

namespace CmdIf
{

namespace V1
{

void CmdSyntaxGraphTest::getNextTokenTest(const char* syntax)
{
	std::cout << "================================================" << std::endl;
	std::cout << std::endl;
	std::cout << "Test: getNextTokenTest" << std::endl;
	std::cout << std::endl;

	
	std::string token;

	std::cout << "Input: " << std::string(syntax) << std::endl;
	std::cout << std::endl;

	std::cout << "Output:" << std::endl;
	while(m_syntaxGraph.getNextToken(syntax, token))
	{
		std::cout << "       Token: " << token << std::endl;
	}
	std::cout << std::endl;
	
	std::cout << "================================================" << std::endl;
	std::cout << std::endl;
}

void CmdSyntaxGraphTest::splitOutSyntaxTest(const char* syntax)
{
	std::shared_ptr<GraphNode> firstNode = std::make_shared<GraphNode>("abc");

	GraphNodeList lastNodes { firstNode };

	std::cout << "Input: " << std::string(syntax) << std::endl;
	std::cout << std::endl;

	std::cout << "Output:" << std::endl;
	if(m_syntaxGraph.splitOutSyntax(syntax, lastNodes) != '\0')
	{
		// ERROR TRACE: Incomplete syntax
		std::cout << "       Failed to splitOutSyntax()!" << std::endl;
		return;
	}
	std::cout << std::endl;
	
	std::cout << "================================================" << std::endl;
	std::cout << std::endl;
}

std::shared_ptr<GraphNode> CmdSyntaxGraphTest::addSyntax(std::shared_ptr<GraphNode> firstNode, const char* syntax, const CmdTypesIf::CmdFunctionWrapper& cmdHandler)
{
	std::cout << "Input: " << std::string(syntax) << std::endl;
	std::cout << std::endl;

	std::cout << "Output:" << std::endl;

	std::cout << "cmdHandler: " << std::hex << &cmdHandler << std::endl;
	m_syntaxGraph.addSyntax(firstNode, std::string(syntax), cmdHandler);
	std::cout << std::endl;
	
	std::cout << "================================================" << std::endl;
	std::cout << std::endl;

	return firstNode;
}

void CmdSyntaxGraphTest::evaluateCommandArguments(std::shared_ptr<GraphNode> firstNode, const std::vector<std::string>& args)
{
	std::shared_ptr<std::ostringstream> validArgs = std::make_shared<std::ostringstream>();
	*validArgs << firstNode->m_name << " ";

	std::cout << "Input: " << (firstNode->m_name).c_str() << std::endl;
	std::cout << std::endl;

	std::cout << "Output:" << std::endl;
	std::shared_ptr<GraphNode> resNode = m_syntaxGraph.evaluateCommandArguments(firstNode, validArgs, args.size() - 1, args.cbegin() + 1);
	
	if(resNode && resNode->m_handler.func)
	{
		std::cout << "[OK]:       CmdHandler was found!" << std::endl;
	}

	std::cout << std::endl;
	
	std::cout << "================================================" << std::endl;
	std::cout << std::endl;
}

void CmdSyntaxGraphTest::printNextPossibleArgumentsTest(std::shared_ptr<GraphNode> currentNode)
{
	std::cout << "Input: " << (currentNode->m_name).c_str() << std::endl;
	std::cout << std::endl;

	std::ostringstream output;
	std::cout << "Output:" << std::endl;
	m_syntaxGraph.printNextPossibleArguments(currentNode, output);
	
	std::cout << "[OK]:       " << output.str() << std::endl;

	std::cout << std::endl;
	
	std::cout << "================================================" << std::endl;
	std::cout << std::endl;
}

CmdTypesIf::CmdResultCode CmdSyntaxGraphTest::mockCmdHandler(const std::vector<std::string>& arguments, std::ostringstream& outputStream)
{
	(void)arguments;
	(void)outputStream;
	std::cout << "cmdHandler1 get called!" << std::endl;
	return CmdTypesIf::CmdResultCode::CMD_RET_SUCCESS;
}

CmdTypesIf::CmdResultCode CmdSyntaxGraphTest::mockCmdHandler2(const std::vector<std::string>& arguments, std::ostringstream& outputStream)
{
	(void)arguments;
	(void)outputStream;
	std::cout << "cmdHandler2 get called!" << std::endl;
	return CmdTypesIf::CmdResultCode::CMD_RET_FAIL;
}

} // namespace V1

} // namespace CmdIf

