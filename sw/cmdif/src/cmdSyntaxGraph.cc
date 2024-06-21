#include <string>
#include <cstring>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <sstream>

#include <traceIf.h>

#include "cmdSyntaxGraph.h"

namespace CmdIf
{

namespace V1
{

GraphNode::GraphNode(const std::string& nodeName)
{
	m_anyValue = strchr(nodeName.c_str(), '<') && strchr(nodeName.c_str(), '>');
	m_name = nodeName;
}

std::shared_ptr<GraphNode> GraphNode::createAndAddNewSubnodeIfNotExist(const std::string& nodeName)
{
	/* If subnode already exists, just return the subnode */
	std::shared_ptr<GraphNode> existingNode = findSubnode(nodeName);
	if(existingNode)
	{
		// TPT_TRACE(TRACE_DEBUG, "Node \"%s\" already exists, return!", nodeName.c_str());
		return existingNode;
	}

	/* If not, create one and add the created subnode to the current node if not exist */
	// TPT_TRACE(TRACE_DEBUG, "Node \"%s\" not exists, create a new one!", nodeName.c_str());
	std::shared_ptr<GraphNode> newNode = std::make_shared<GraphNode>(nodeName);
	return addCreatedSubnodeIfNotExist(newNode);
}

std::shared_ptr<GraphNode> GraphNode::addCreatedSubnodeIfNotExist(std::shared_ptr<GraphNode>& createdSubnode)
{
	if(findSubnode(createdSubnode->m_name))
	{
		return createdSubnode;
	}

	// TPT_TRACE(TRACE_DEBUG, "Add a created node \"%s\" to node \"%s\"", (createdSubnode->m_name).c_str(), m_name.c_str());
	m_subNodeList.push_back(createdSubnode);
	return createdSubnode;
}

std::shared_ptr<GraphNode> GraphNode::findSubnode(const std::string& nodeName)
{
	for(const auto& node : m_subNodeList)
	{
		if(node->m_name == nodeName)
		{
			return node;
		}
	}

	// TPT_TRACE(TRACE_DEBUG, "Could not find subnode \"%s\"", nodeName.c_str());
	return nullptr;
}

GraphNodeList GraphNode::getMatchingSubnodes(const std::string& nodeName)
{
	/* Prefer seeking for exactly matching node name */
	for(const auto& node : m_subNodeList)
	{
		if(!node->m_anyValue && node->m_name == nodeName)
		{
			return { node };
		}
	}

	/* If not, return all any_value nodes */
	GraphNodeList matchingSubnodeList;
	for(const auto& node : m_subNodeList)
	{
		if(node->m_anyValue)
		{
			matchingSubnodeList.push_back(node);
		}
	}

	return matchingSubnodeList;
}

bool CmdSyntaxGraph::getNextToken(const char*& syntax, std::string& token)
{
	/* arg1 arg2 	arg3 arg4 ... 
	            ^
                    |
		    *syntax pointer is pointing to here

		    Skip white spaces and tabs to come to the next argument, for example, the start of arg3 as above.
	*/
	while(*syntax && strchr(" \t", *syntax))
	{
		syntax++;
	}

	/* arg1 { arg2 | [ arg3 ] } arg4 ... 
		^      ^ ^
                |      | |
		*syntax pointer is either pointing to these places

		Capture special characters.
	*/
	if(*syntax && strchr("{[|]}", *syntax))
	{
		token = *syntax++;
		return true;
	}

	/* Or token is an actual argument */
	token.clear(); // Clear the previous token
	int angleBracketFlag = 0; // To see if this actual argument is an any value which is inside < >
	while(*syntax && (angleBracketFlag > 0 || !strchr(" \t{[|]}", *syntax)))
	{
		// Note that: Node name of nodes where m_anyValue is set will be in form of "<abc>" for example,
		// so when constructor of GraphNode call, m_anyValue should be set since '<' and '>' found in node name
		angleBracketFlag += (*syntax == '<') ? 1 : (*syntax == '>' ? -1 : 0);
		token += *syntax++;
	}

	return (token.length() > 0);
}

char CmdSyntaxGraph::splitOutSyntax(const char*& syntax, GraphNodeList& lastNodes)
{
	std::string token;

	/* The final aim is to split out all arguments in a syntax for example like this:
	>> abc { def ghi | jkl [ mno | pqr ] } stu 123
	*/
	while(getNextToken(syntax, token))
	{
		char c = token[0];

		/* Start a collection of some new nodes which is inside { ... | ... } or [ ... | ... ] */
		if(c == '{' || c == '[')
		{
			GraphNodeList newLastNodes; // All nodes in this collection will be the new last nodes

			// TPT_TRACE(TRACE_DEBUG, "Received token \"%s\", analyzing inside it!", c == '{' ? "{" : "[");
			/* If it's [ ] instead of { }, nodes inside [ ] will be considered as optional, so included in the current last node list */
			if(c == '[')
			{
				newLastNodes = lastNodes;
			}

			// printGraphNodeList(lastNodes); // DEBUG ONLY

			do
			{
				// Make a copy of the current lastNodes to provide into a recursive call of splitOutSyntax()
				// to process nodes inside { } or [ ]
				// TPT_TRACE(TRACE_DEBUG, "Received token \"%s\", check if continue receiving!", c == '|' ? "|" : "nothing");
				GraphNodeList copiedLastNodes = lastNodes;

				// Process nodes inside { } or [ ] one by one which are separated out by '|'
				/* For example: abc { def ghi | jkl [ mno ] }
						    ^
						    |
						    *syntax is pointing to here

					After the splitOutSyntax() call below, we will have copiedLastNodes with nodes "def" and "ghi",
					then new while() loop for the next alternative arguments which are in between of special characters '|'
				*/
				c = splitOutSyntax(syntax, copiedLastNodes);
				for(const auto& node : copiedLastNodes)
				{
					/* After done push all nodes inside { } or [ ] to our newLastNodes in the current context */
					if(std::find(newLastNodes.begin(), newLastNodes.end(), node) == newLastNodes.end())
					{
						newLastNodes.push_back(node);
					} else
					{
						TPT_TRACE(TRACE_ABN, "Ignore node \"%s\" which is already exist, please recheck syntax!", (node->m_name).c_str());
					}
				}

				// printGraphNodeList(newLastNodes); // DEBUG ONLY
			} while(c == '|'); // Continue for all child collections inside { } or [ ] until reaching out '}' or ']'

			// printGraphNodeList(newLastNodes); // DEBUG ONLY
			lastNodes = newLastNodes; // Assign back new last nodes to "lastNodes" to update the "lastNodes" of current context
		}
		else if(c == '|' || c == '}' || c == ']')
		{
			// TPT_TRACE(TRACE_INFO, "Received token \"%s\", continue analyzing syntax!", c == '|' ? "|" : c == '}' ? "}" : "]" );
			return c;
		}
		else
		{
			if(lastNodes.size() == 1 && lastNodes[0]->m_name == token)
			{
				/* No need to add a node that has a same name with cmdName, and also is the first argument which should have been created by addSyntax()
				   Or by some weird situation, two consecutive arguments have a same name, ignore the latter
				   INFO TRACE: is needed here to print out lastNodes[0]->m_name and token */
				// TPT_TRACE(TRACE_DEBUG, "Token: \"%s\" (to be skipped)", token.c_str());
				continue;
			}
			else
			{
				/* A single successive node which should be linked to all current last nodes */
				// TPT_TRACE(TRACE_DEBUG, "Token: \"%s\" (append to lastnodes below)", token.c_str());
				// printGraphNodeList(lastNodes); // DEBUG ONLY
				appendNewNodeToLastNodes(token, lastNodes);
			}
		}
	}

	TPT_TRACE(TRACE_INFO, "Done splitOutSyntax()!");
	// TPT_TRACE(TRACE_DEBUG, "Last nodes below will be attached with cmdHandler!");
	// printGraphNodeList(lastNodes); // DEBUG ONLY
	return '\0'; // Indicate that we have done spliting out a syntax
}

void CmdSyntaxGraph::appendNewNodeToLastNodes(const std::string& nodeName, GraphNodeList& lastNodes)
{
	std::shared_ptr<GraphNode> newNode;

	for(const auto& node : lastNodes)
	{
		/* */
		if(!newNode || newNode == node)
		{
			newNode = node->createAndAddNewSubnodeIfNotExist(nodeName);
		}
		else
		{
			node->addCreatedSubnodeIfNotExist(newNode);
		}
	}

	// After append newNode to all last nodes in the lastNodes list, set lastNodes to the just-appended one
	lastNodes.clear();
	lastNodes.push_back(newNode);
}

bool CmdSyntaxGraph::validateBrackets(const std::string& syntax, char openBracket, char closeBracket)
{
	int checkBrackets = 0;
	
	for(const auto& c : syntax)
	{
		checkBrackets += (c == openBracket) ? +1 : (c == closeBracket) ? -1 : 0;
	}

	if(checkBrackets)
	{
		TPT_TRACE(TRACE_ERROR, "Syntax \"%s\" has unbalanced brackets %s!", syntax.c_str(), openBracket == '{' ? "{}" : openBracket == '[' ? "[]" : "<>");
		return false;
	}

	return true;
}

void CmdSyntaxGraph::addSyntax(std::shared_ptr<GraphNode>& firstNode, const std::string& syntax, const CmdTypesIf::CmdFunctionWrapper& cmdHandler)
{
	if(!validateBrackets(syntax, '{', '}') || !validateBrackets(syntax, '[', ']') || !validateBrackets(syntax, '<', '>'))
	{
		return;
	}

	GraphNodeList lastNodes { firstNode };
	const char* s = syntax.c_str();
	if(splitOutSyntax(s, lastNodes) != '\0')
	{
		TPT_TRACE(TRACE_ERROR, "Failed to split out syntax for cmdName \"%s\"", (firstNode->m_name).c_str());
		return;
	}

	// Assign cmdHandler to the last nodes
	for(auto& node : lastNodes)
	{
		if(node->m_handler.func == nullptr && node->m_handler.funcName == "")
		{
			TPT_TRACE(TRACE_INFO, "Attaching cmdHandler \"%s\" to node \"%s\", first time!", (cmdHandler.funcName).c_str(), (node->m_name).c_str());
		} else if(node->m_handler.funcName != cmdHandler.funcName)
		{
			TPT_TRACE(TRACE_ABN, "Overwrite existing cmdHandler on node \"%s\", old cmdHandler \"%s\", new cmdHandler \"%s\"", (node->m_name).c_str(), (node->m_handler.funcName).c_str(), (cmdHandler.funcName).c_str());
		} else
		{
			TPT_TRACE(TRACE_INFO, "This node \"%s\" already holds cmdHandler \"%s\", will not re-assign!", (node->m_name).c_str(), (node->m_handler.funcName).c_str());
			continue;
		}

		node->m_handler = cmdHandler;
	}
}

void CmdSyntaxGraph::addCommand(const std::string& cmdName, const std::vector<std::pair<std::string, CmdTypesIf::CmdFunctionWrapper>>& syntaxHandler)
{
	std::scoped_lock<std::mutex> lock(m_mutex);

	const auto& cmd = m_cmdMap.find(cmdName);
	if(cmd != m_cmdMap.cend())
	{
		// ABN TRACE: cmdName already added in the CmdTableIf
		return;
	}

	std::shared_ptr<GraphNode> firstNode = std::make_shared<GraphNode>(cmdName);

	for(const auto& sh : syntaxHandler)
	{
		addSyntax(firstNode, sh.first, sh.second);
	}

	m_cmdMap.emplace(std::make_pair(cmdName, firstNode));
	
}

std::shared_ptr<GraphNode> CmdSyntaxGraph::evaluateCommandArguments(std::shared_ptr<GraphNode> currentNode, std::shared_ptr<std::ostringstream> validArgs, size_t numArgs, std::vector<std::string>::const_iterator args) const
{
	// Go through all actual arguments (excluded the first argument which is the cmdName as well)
	// The first argument should be already checked by findCmdHandler()
	for(size_t i = 0; i < numArgs; ++i)
	{
		GraphNodeList subnodes = currentNode->getMatchingSubnodes(args[i]);

		size_t numSubnodes = subnodes.size();
		if(numSubnodes == 0)
		{
			// No matching subnodes found in this potential path: skip all remaining arguments
			TPT_TRACE(TRACE_ABN, "No matching node found, return current node \"%s\"!", (currentNode->m_name).c_str());
			return currentNode;
		}
		else if(numSubnodes == 1)
		{
			// Only one matching subnode: this is a string literal argument or this is a anyValue argument
			if(subnodes[0]->m_handler.func && numArgs == i + 1)
			{
				// check if it's the last one in the path which should have the cmdHandler
				TPT_TRACE(TRACE_INFO, "Found cmdHandler on node \"%s\"!", (subnodes[0]->m_name).c_str());
				return subnodes[0]; // cmdHandler found, finish!
			}

			// If not, continue to the next argument
			currentNode = subnodes[0];
			if(validArgs)
			{
				*validArgs << currentNode->m_name << " ";
			}
		}
		else
		{
			// Multiple matching subnodes (these are anyValue nodes)
			for(const auto& subnode : subnodes)
			{
				// If subnode has no any subnodes, return itself (see the first "if" case), then we can check if cmdHander is hold by it below
				// std::shared_ptr<GraphNode> sn = evaluateCommandArguments(subnode, validArgs, numArgs - (i + 1), args + (i + 1));
				std::shared_ptr<GraphNode> sn = evaluateCommandArguments(subnode, nullptr, numArgs - (i + 1), args + (i + 1)); // First version
				if(sn && sn->m_handler.func)
				{
					TPT_TRACE(TRACE_INFO, "Found cmdHandler on deeper node \"%s\"!", (sn->m_name).c_str());
					return sn; // cmdHandler found, finish!
				}
			}

			// Even if after digging deeper into all above subnodes's path we cannot find the last node that contains cmdHandler, we must give up here!
			if(validArgs)
			{
				*validArgs << args[i] << " ";
			}
			
			TPT_TRACE(TRACE_ABN, "Could not find cmdHandler, return default node \"%s\"!", (subnodes[0]->m_name).c_str());
			return subnodes[0]; // If failed, by default choose the first subnode of subnodes to start printing a correct syntax
		}
	}

	// All actual arguments have been checked and just found one path that match those arguments
	TPT_TRACE(TRACE_INFO, "Return with node \"%s\"!", (currentNode->m_name).c_str());
	return currentNode;
}

const std::shared_ptr<CmdTypesIf::CmdFunctionWrapper> CmdSyntaxGraph::findCmdHandler(size_t numArgs, std::vector<std::string>::const_iterator args, std::ostringstream& output) const
{
	std::scoped_lock<std::mutex> lock(m_mutex);
	
	const auto& firstNodeIter = m_cmdMap.find(args[0]);
	if(firstNodeIter == m_cmdMap.cend())
	{
		// ABN TRACE: cmdName not found in CmdTableIf
		return nullptr;
	}
	
	std::shared_ptr<std::ostringstream> validArgs = std::make_shared<std::ostringstream>();
	*validArgs << firstNodeIter->first << " ";

	std::shared_ptr<GraphNode> node = evaluateCommandArguments(firstNodeIter->second, validArgs, numArgs - 1, args + 1);

	if(node && node->m_handler.func)
	{
		return std::make_shared<CmdTypesIf::CmdFunctionWrapper>(node->m_handler);
	}
	else
	{
		output << "Usage: " << (*validArgs).str();
		// Continuously add the correct syntax after validArgs to the output
		printNextPossibleArguments(node, output);
		output << "\n\n";

		return nullptr;
	}

}

void CmdSyntaxGraph::printNextPossibleArguments(std::shared_ptr<GraphNode> currentNode, std::ostringstream& output)
{
	while(currentNode)
	{
		size_t numSubnodes = currentNode->m_subNodeList.size();

		if(numSubnodes == 0)
		{
			break;
		}
		else if(numSubnodes == 1)
		{
			/* Single child 
									-> B has no child : Not possible, since B must contain cmdHandler!
									|
			Case 1:	... 	->	A	->	B 	-> B has single child (same as this case, will be recursively taken into account)
									|
									-> B has mutilple children : ... A B { ... | ... }

									-> B has no child : Not possible, since B must contain cmdHandler!
									|
			Case 2:	... 	->	(A)	->	B 	-> B has single child (same as this case, will be recursively taken into account)
									|
									-> B has mutilple children : ... A [ B { ... | ... } ]
			
									-> B has no child : ... A B
									|
			Case 3:	... 	->	A	->	(B) 	-> B has single child (same as this case, will be recursively taken into account)
									|
									-> B has mutilple children : ... A B [ { ... | ... } ] 
				
									-> B has no child : ... A [ B ]
									|
			Case 4:	... 	->	(A)	->	(B) 	-> B has single child (same as this case, will be recursively taken into account)
									|
									-> B has mutilple children : ... A [ B [ { ... | ... } ] ]

			=> Possible Syntax:
				1. ... A B { ... | ... }
				2. ... A [ B { ... | ... } ]
				3. ... A B
				4. ... A B [ { ... | ... } ]
				5. ... A [ B ]
				6. ... A [ B [ { ... | ... } ] ]

			Note that: (A) meaning A is a node that contains cmdHandler
			*/

			CmdTypesIf::CmdFunctionWrapper prevHandler = currentNode->m_handler;
			currentNode = currentNode->m_subNodeList[0];

			if(prevHandler.func && currentNode->m_handler.func && currentNode->m_subNodeList.size() == 0)
			{
				// Possible syntax 5 -> case 3 above
				output << "[ " << currentNode->m_name << " ] ";
			}
			else
			{
				output << currentNode->m_name << " ";
			}
		}
		else if(numSubnodes == 2 &&
			currentNode->m_subNodeList[0]->m_subNodeList.size() &&
			currentNode->m_subNodeList[0]->m_subNodeList[0] == currentNode->m_subNodeList[1])
		{
			/*
					---------------------------------
					|				|
					|				v
				... 	A	->	B	->	C	->	...

				=> Possible situation:
					1. A B C
					2. A C

				=> Syntax: ... A [ B ] C
				*/

			const std::string& optKey = currentNode->m_subNodeList[0]->m_name;
			currentNode = currentNode->m_subNodeList[1];
			output << "[ " << optKey << " ] " << currentNode->m_name << " ";
		}
		else
		{
			std::string orKey { "" };
			output << "{ ";

			for(const auto& node : currentNode->m_subNodeList)
			{
				output << orKey << node->m_name;

				if(node != currentNode->m_subNodeList[currentNode->m_subNodeList.size() - 1])
				{
					output << " ";
				}

				orKey = "| ";
			}

			output << " }";
			break;
		}
	}
}

void CmdSyntaxGraph::printGraphNodeList(const GraphNodeList& list)
{
	TPT_TRACE(TRACE_INFO, "This GraphNodeList has %lu nodes!", list.size());
	for(const auto& node : list)
	{
		TPT_TRACE(TRACE_INFO, "Node: \"%s\"", (node->m_name).c_str());
	}
}

} // namespace V1

} // namespace CmdIf