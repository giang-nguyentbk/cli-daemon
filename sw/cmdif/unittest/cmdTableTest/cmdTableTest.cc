#include <iostream>
#include <vector>
#include <functional>
#include <memory>
#include <sstream>

#include "cmdTableIf.h"

using namespace CmdIf::V1;

CmdTypesIf::CmdResultCode mockCmdHandler1(const std::vector<std::string>& arguments, std::ostringstream& outputStream)
{
	(void)arguments;

	std::cout << "cmdHandler1 get called!" << std::endl;
	outputStream << "I'm here in mockCmdHandler1\n";
	return CmdTypesIf::CmdResultCode::CMD_RET_SUCCESS;
}

CmdTypesIf::CmdResultCode mockCmdHandler2(const std::vector<std::string>& arguments, std::ostringstream& outputStream)
{
	(void)arguments;

	std::cout << "cmdHandler2 get called!" << std::endl;
	outputStream << "I'm here in mockCmdHandler2\n";
	return CmdTypesIf::CmdResultCode::CMD_RET_FAIL;
}

int main()
{
	CmdTableIf& cmdTableIf = CmdTableIf::getInstance();

	const std::string& cmdName { "abc" };
	std::vector<CmdTypesIf::CmdDefinition> m_abcCmdDefinitions {
		{
			"abc def [ ghi | jkl ] { mno | <pqr> } stu",
			{
				std::bind(&mockCmdHandler1, std::placeholders::_1, std::placeholders::_2),
				"mockCmdHandler1"
			},
			"This is a mock command definition 1!"
		},
		{
			"abc <bcd> kli",
			{
				std::bind(&mockCmdHandler2, std::placeholders::_1, std::placeholders::_2),
				"mockCmdHandler2"
			},
			"This is a mock command definition 2!"
		}
	};

	cmdTableIf.registerCmdTable(cmdName, m_abcCmdDefinitions);

	std::vector<std::string> args {"abc", "def", "111", "stu"};
	std::ostringstream output;
	cmdTableIf.executeCmd(args, output);
	std::cout << output.str() << std::endl;

	args = {"abc", "def"};
	output.clear();
	cmdTableIf.executeCmd(args, output);
	std::cout << output.str() << std::endl;

	output.clear();
	cmdTableIf.printCmdHelp(m_abcCmdDefinitions, output);
	std::cout << output.str() << std::endl;

	return 0;
}