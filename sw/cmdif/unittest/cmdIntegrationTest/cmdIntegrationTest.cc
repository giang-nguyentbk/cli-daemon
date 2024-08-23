#include <iostream>
#include <vector>
#include <functional>
#include <memory>
#include <sstream>
#include <cstdlib>

#include <itc.h>
#include <itcPubSubIf.h>
#include <eventLoopIf.h>
#include <traceIf.h>
#include <stringUtils.h>

#include "cli-daemon-tpt-provider.h"
#include "cmdJobIf.h"
#include "cmdRegisterIf.h"
#include "cmdTableIf.h"
#include "cmdTypesIf.h"

using namespace CmdIf::V1;
using namespace CommonUtils::V1::StringUtils;

CmdTypesIf::CmdResultCode handleCmd1(const std::vector<std::string>& arguments, std::ostringstream& outputStream);
CmdTypesIf::CmdResultCode handleCmdHelp(const std::vector<std::string>& arguments, std::ostringstream& outputStream);
void abcCmdHandler(const std::shared_ptr<CmdIf::V1::CmdJobIf>& job);
void atexit_handler();

using TaskCompleteCallback = std::function<void(bool)>;
TaskCompleteCallback m_taskCompleteCallback;
std::shared_ptr<CmdIf::V1::CmdJobIf> m_currentJob;
itc_mbox_id_t m_cmdIntegrationTestMboxId { ITC_NO_MBOX_ID };

const std::string ABC_CMD { "abc" };
const std::string m_cmdDesc { "This is description for cmd \"abc\"" };
std::vector<CmdTypesIf::CmdDefinition> m_abcCmdDefinitions {
	{
		"abc <ghi> [ <def> ]",
		{
			std::bind(&handleCmd1, std::placeholders::_1, std::placeholders::_2),
			"handleCmd1"
		},
		"This is a mock command definition 1!"
	},
	{
		"abc { help | --help | -h }",
		{
			std::bind(&handleCmdHelp, std::placeholders::_1, std::placeholders::_2),
			"handleCmdHelp"
		},
		"This is a mock help command definition!"
	}
};

int main()
{
	if(itc_init(3, ITC_MALLOC, 0) == false)
	{
		TPT_TRACE(TRACE_ERROR, SSTR("Failed to itc_init() by cmdIntegrationTest!"));
		return false;
	}

	m_cmdIntegrationTestMboxId = itc_create_mailbox("cmdTestMailbox", ITC_NO_NAMESPACE);
	if(m_cmdIntegrationTestMboxId == ITC_NO_MBOX_ID)
	{
		TPT_TRACE(TRACE_ERROR, SSTR("Failed to create mailbox \"cmdTestMailbox\"!"));
		return false;
	}

	std::atexit(atexit_handler);

	UtilsFramework::ItcPubSub::V1::IItcPubSub::getThreadLocalInstance().addItcFd(itc_get_fd());
	CmdTableIf::getInstance().registerCmdTable(ABC_CMD, m_abcCmdDefinitions);
	CmdRegisterIf::getInstance().registerCmdHandler(ABC_CMD, m_cmdDesc, std::bind(&abcCmdHandler, std::placeholders::_1));

	UtilsFramework::EventLoop::V1::IEventLoop::getThreadLocalInstance().run();

	return 0;
}

CmdTypesIf::CmdResultCode handleCmd1(const std::vector<std::string>& arguments, std::ostringstream& outputStream)
{
	TPT_TRACE(TRACE_INFO, SSTR("CMD: Handle cmd1!"));

	if(arguments.size() != 2 && arguments.size() != 3)
	{
		return handleCmdHelp(arguments, outputStream);
	}

	auto ghi = CommonUtils::V1::StringUtils::stringToIntegralType<uint32_t>(arguments[1]);

	if(!ghi.has_value())
	{
		outputStream << "Invalid \"<ghi>\" parameter: " << arguments[1] << "\n";
		outputStream << "Usage: abc <ghi> [ <def> ]\n";
		return CmdTypesIf::CmdResultCode::CMD_RET_FAIL;
	}

	auto def = (arguments.size() == 3 ? (arguments[2] == "wait") : false);
	(void)def;

	/* Trigger the whole process (function like startTask()), spread starting actions and also give the current job (to be able to modify outputStream)
	to all classes in the chain (see Chain of Responsibility design pattern)
	or send message/signal to start processes on other threads,... That is, the entire process might take some time.'
	So will act as an asynchronous manner by storing the job here, spread starting tasks, immediately return handCmd1 here, when the the whole process
	is done, will have a previously stored callback to call job->done() then. */

	/* Make a copy of current job, and clear m_currentJob to let other job later can start immediately in an asynchronous way */\
	auto job = m_currentJob;
	if(job == nullptr)
	{
		TPT_TRACE(TRACE_ERROR, SSTR("Current job m_currentJob is nullptr!\n"));
		return CmdTypesIf::CmdResultCode::CMD_RET_FAIL;
	}

	// Lamda's capture clause will make a copy of job to preserve it later on when taskCompleteCallback getting called
	m_taskCompleteCallback = [job](bool isSuccess)
	{
		if(!isSuccess)
		{
			job->getOutputStream() << "Execute cmd " << job->getCmdName() << " failed!\n";
		}
		else
		{
			job->getOutputStream() << "Execute cmd " << job->getCmdName() << " successfully!\n";
		}

		job->done(isSuccess ? CmdTypesIf::CmdResultCode::CMD_RET_SUCCESS : CmdTypesIf::CmdResultCode::CMD_RET_FAIL);
	};

	// Here we simulate the whole task done successfully immediately in a synchrous manner
	m_taskCompleteCallback(true);

	job.reset();
	m_currentJob.reset(); // Reset shared_ptr to nullptr to make other job can continue just immediately
	return CmdTypesIf::CmdResultCode::CMD_RET_SUCCESS;
}

CmdTypesIf::CmdResultCode handleCmdHelp(const std::vector<std::string>& arguments, std::ostringstream& outputStream)
{
	TPT_TRACE(TRACE_INFO, SSTR("CMD: Handle printing help!"));

	if(arguments[0] == ABC_CMD)
	{
		CmdTableIf::getInstance().printCmdHelp(m_abcCmdDefinitions, outputStream);
	}
	else
	{
		TPT_TRACE(TRACE_ERROR, SSTR("CMD: Invalid command name \"", arguments[0], "\""));
		outputStream << "Invalid command name " << arguments[0] << "\n";
	}

	return CmdTypesIf::CmdResultCode::CMD_RET_INVALID_ARGS;
}

void abcCmdHandler(const std::shared_ptr<CmdIf::V1::CmdJobIf>& job)
{
	if(m_currentJob)
	{
		job->getOutputStream() << "Another job still running!\n";
		job->done(CmdTypesIf::CmdResultCode::CMD_RET_FAIL);
		return;
	}

	m_currentJob = job;
	CmdTypesIf::CmdResultCode res = CmdTableIf::getInstance().executeCmd(job->getArguments(), job->getOutputStream());

	/* If cmdInvoker could not be called, that's invalid arguments being given, so handleCmd1 was not called */
	if(m_currentJob)
	{
		m_currentJob->done(res);
		m_currentJob.reset();
	}
}

void atexit_handler()
{
	UtilsFramework::EventLoop::V1::IEventLoop::getThreadLocalInstance().stop();

	itc_delete_mailbox(m_cmdIntegrationTestMboxId);
	itc_exit();
}









