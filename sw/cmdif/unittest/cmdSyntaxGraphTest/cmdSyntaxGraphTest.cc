#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <string>

#include "cmdSyntaxGraphTest.h"


namespace CmdIf
{

namespace V1
{

bool CmdSyntaxGraphTest::getNextTokenTest(const char*& syntax, std::string& token)
{
	return m_syntaxGraph.getNextToken(syntax, token);
}

	
} // namespace V1

} // namespace CmdIf

