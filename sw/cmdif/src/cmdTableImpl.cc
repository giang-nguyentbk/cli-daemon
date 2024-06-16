#include "cmdTableImpl.h"
#include "cmdTypesIf.h"

namespace CmdIf
{

namespace V1
{

void CmdTableImpl::registerCmdTable(const std::string& cmdName, const std::vector<CmdTypesIf::CmdDefinition>& cmdDefinitions)
{
	std::vector<std::pair<std::string, CmdTypesIf::CmdFunction>> syntaxes;
	syntaxes.reserve(cmdDefinitions.size());
	for(auto& cmdDef : cmdDefinitions)
	{
		syntaxes.emplace_back(cmdDef.m_syntax, cmdDef.m_handler);
	}

	m_syntaxGraph.addCommand(cmdName, syntaxes);
}

CmdIf::V1::CmdTypesIf::CmdResultCode CmdTableImpl::executeCmd(const std::vector<std::string>& args, std::string& output)
{
	auto res = CmdTypesIf::CmdResultCode::CMD_RET_FAIL;

	const std::shared_ptr<CmdTypesIf::CmdFunction> handler = m_syntaxGraph.findCmdHandler(args.size(), args.cbegin(), output);
	if(handler)
	{
		// INFO TRACE: Executing command: 
		res = (*handler)(args, output);
	}

	return res;
}

void CmdTableImpl::printCmdHelp(const std::vector<CmdTypesIf::CmdDefinition>& cmdDefinitions, std::string& output)
{
	static const std::string INDENT { "\n     " };

	output += "Comman syntax:\n\n";

	for(const auto& cmdDef : cmdDefinitions)
	{
		output += "  ";
		output += cmdDef.m_syntax;
		output += "\n";
		output += INDENT;
		output += cmdDef.m_desc;
		output += "\n\n";
	}
}

} // namespace V1

} // namespace CmdIf