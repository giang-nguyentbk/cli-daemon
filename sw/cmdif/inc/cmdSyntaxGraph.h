/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#pragma once

#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <string>
#include <mutex>

#include "cmdTableIf.h"

namespace CmdIf
{

namespace V1
{

class GraphNode;

using GraphNodeList = std::vector<std::shared_ptr<GraphNode>>;

class GraphNode
{
public:
	GraphNodeList			m_subNodeList;
	CmdTableIf::CmdFunction		m_handler; // Only the last nodes of each graph hold the actual handler
	bool				m_anyValue;
	std::string			m_name;

	explicit GraphNode(const std::string& nodeName);
	virtual ~GraphNode() = default;

	std::shared_ptr<GraphNode> createAndAddNewSubnodeIfNotExist(const std::string& nodeName);
	std::shared_ptr<GraphNode> addCreatedSubnodeIfNotExist(std::shared_ptr<GraphNode>& createdSubnode);
	std::shared_ptr<GraphNode> findSubnode(const std::string& nodeName);
	GraphNodeList getMatchingSubnodes(const std::string& nodeName);

	GraphNode(const GraphNode&) = delete;
	GraphNode(GraphNode&&) = delete;
	GraphNode& operator=(const GraphNode&) = delete;
	GraphNode& operator=(GraphNode&&) = delete;


}; // class GraphNode

class CmdSyntaxGraph
{
public:
	void addCommand(const std::string& cmdName, const std::vector<std::pair<std::string, CmdTableIf::CmdFunction>>& syntaxHandler);
	const std::shared_ptr<CmdTableIf::CmdFunction> executeCmdHandler(size_t numArgs, std::vector<std::string>::const_iterator args, std::string& output) const;

	CmdSyntaxGraph() = default;
	virtual ~CmdSyntaxGraph() = default;

	CmdSyntaxGraph(const CmdSyntaxGraph&) = delete;
	CmdSyntaxGraph(CmdSyntaxGraph&&) = delete;
	CmdSyntaxGraph& operator=(const CmdSyntaxGraph&) = delete;
	CmdSyntaxGraph& operator=(CmdSyntaxGraph&&) = delete;



private:
	static bool getNextToken(const char*& syntax, std::string& token);
	static char splitOutSyntax(const char*& syntax, GraphNodeList& lastNodes);
	static void appendNewNodeToLastNodes(const std::string& nodeName, GraphNodeList& lastNodes);
	static bool validateBrackets(const std::string& syntax, char openBracket, char closeBracket);
	void addSyntax(std::shared_ptr<GraphNode>& firstNode, const std::string& syntax, const CmdTableIf::CmdFunction& cmdHandler);
	std::shared_ptr<GraphNode> evaluateCommandArguments(std::shared_ptr<GraphNode> currentNode, std::shared_ptr<std::string> validArgs, size_t numArgs, std::vector<std::string>::const_iterator args) const;
	static void printfCorrectSyntax(std::shared_ptr<GraphNode> currentNode, std::string& output);

private:
	std::map<std::string, std::shared_ptr<GraphNode>> m_cmdMap; // Contain a list of graph trees with respective cmdNames
	mutable std::mutex m_mutex; // To protect m_cmdMap when multiple threads want to add their cmds to

}; // class CmdSyntaxGraph


} // namespace V1

} // namespace CmdIf