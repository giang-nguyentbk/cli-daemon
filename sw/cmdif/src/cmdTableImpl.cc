#include "cmdTableImpl.h"
#include "cmdTypesIf.h"

namespace CmdIf
{

namespace V1
{

CmdTableIf& CmdTableIf::getInstance()
{
	return CmdTableImpl::getInstance();
}

CmdTableImpl& CmdTableImpl::getInstance()
{
	static CmdTableImpl m_instance;
	return m_instance;
}

void CmdTableImpl::registerCmdTable(const std::string& cmdName, const std::vector<CmdTypesIf::CmdDefinition>& cmdDefinitions)
{
	std::vector<std::pair<std::string, CmdTypesIf::CmdFunctionWrapper>> syntaxes;
	syntaxes.reserve(cmdDefinitions.size());
	for(auto& cmdDef : cmdDefinitions)
	{
		syntaxes.emplace_back(cmdDef.m_syntax, cmdDef.m_handler);
	}

	m_syntaxGraph.addCommand(cmdName, syntaxes);
}

CmdIf::V1::CmdTypesIf::CmdResultCode CmdTableImpl::executeCmd(const std::vector<std::string>& args, std::ostringstream& output)
{
	auto res = CmdTypesIf::CmdResultCode::CMD_RET_INVALID_ARGS;

	const std::shared_ptr<CmdTypesIf::CmdFunctionWrapper> handler = m_syntaxGraph.findCmdHandler(args.size(), args.cbegin(), output);
	if(handler && (*handler).func)
	{
		// INFO TRACE: Executing command: 
		res = (*handler).func(args, output); // This will decide whether or not CMD_RET_FAIL
	}

	return res;
}

void CmdTableImpl::printCmdHelp(const std::vector<CmdTypesIf::CmdDefinition>& cmdDefinitions, std::ostringstream& output)
{
	static const std::string INDENT { "\n     " };

	output << "Command syntax:\n\n";

	for(const auto& cmdDef : cmdDefinitions)
	{
		output << "  >> " << cmdDef.m_syntax << INDENT << cmdDef.m_desc << "\n\n";
	}
}

} // namespace V1

} // namespace CmdIf