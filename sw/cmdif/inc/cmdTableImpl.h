/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#pragma once

#include "cmdTableIf.h"
#include "cmdSyntaxGraph.h"

namespace CmdIf
{

namespace V1
{

class CmdTableImpl : public CmdTableIf
{
public:
	static CmdTableImpl& getInstance();

	void registerCmdTable(const std::string& cmdName, const std::vector<CmdTypesIf::CmdDefinition>& cmdDefinitions) override;
	CmdIf::V1::CmdTypesIf::CmdResultCode executeCmd(const std::vector<std::string>& args, std::string& output) override;
	void printCmdHelp(const std::vector<CmdTypesIf::CmdDefinition>& cmdDefinitions, std::string& output) override;

	CmdTableImpl() = default;
	virtual ~CmdTableImpl() = default;

	CmdTableImpl(const CmdTableImpl&) = delete;
	CmdTableImpl(CmdTableImpl&&) = delete;
	CmdTableImpl& operator=(const CmdTableImpl&) = delete;
	CmdTableImpl& operator=(CmdTableImpl&&) = delete;

private:
	CmdSyntaxGraph m_syntaxGraph;

}; // class CmdTableImpl

} // namespace V1

} // namespace CmdIf