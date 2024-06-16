#include <iostream>
#include <string>

#include "cmdSyntaxGraphTest.h"


using namespace CmdIf::V1;

int main()
{
	CmdSyntaxGraphTest syntaxGraphTest;

	std::cout << "================================================" << std::endl;
	std::cout << std::endl;
	std::cout << "Test: getNextTokenTest" << std::endl;
	std::cout << std::endl;

	const char* syntax1 = "abc { def | ghi } jkl";
	std::string token;

	std::cout << "Input: " << std::string(syntax1) << std::endl;
	std::cout << std::endl;

	std::cout << "Output:" << std::endl;
	while(syntaxGraphTest.getNextTokenTest(syntax1, token))
	{
		std::cout << "       Token: " << token << std::endl;
	}
	std::cout << std::endl;
	
	std::cout << "================================================" << std::endl;
	std::cout << std::endl;

	return 0;
}