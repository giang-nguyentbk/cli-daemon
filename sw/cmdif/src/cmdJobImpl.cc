#include <vector>
#include <string>
#include <cstring>

#include <itc.h>
#include <traceIf.h>
#include <stringUtils.h>

#include "cli-daemon-tpt-provider.h"
#include "cmdJobImpl.h"
#include "cmdTypesIf.h"
#include "cmdProto.h"

using namespace CommonUtils::V1::StringUtils;

namespace CmdIf
{

namespace V1
{

CmdJobImpl::CmdJobImpl(const std::string& cmdName, const unsigned long long jobId, const std::vector<std::string>& args, itc_mbox_id_t clidMboxId)
	: m_cmdName(cmdName),
	  m_jobId(jobId),
	  m_args(args),
	  m_clidMboxId(clidMboxId)
{
}

CmdJobImpl::~CmdJobImpl()
{
}

void CmdJobImpl::done(const CmdIf::V1::CmdTypesIf::CmdResultCode& rc)
{
	uint32_t result;
	if(rc == CmdIf::V1::CmdTypesIf::CmdResultCode::CMD_RET_SUCCESS)
	{
		result = CMDIF_RET_SUCCESS;
	} else if(rc == CmdIf::V1::CmdTypesIf::CmdResultCode::CMD_RET_INVALID_ARGS)
	{
		result = CMDIF_RET_INVALID_ARGS;
	} else if(rc == CmdIf::V1::CmdTypesIf::CmdResultCode::CMD_RET_FAIL)
	{
		result = CMDIF_RET_FAIL;
	} else
	{
		TPT_TRACE(TRACE_ERROR, SSTR("Unknown CmdResultCode rc = ", (char)rc));
		return;
	}

	uint32_t len = m_output.str().length();
	union itc_msg* rep = itc_alloc(offsetof(struct CmdIfExeCmdReplyS, output) + len + 1, CMDIF_EXE_CMD_REPLY);
	rep->cmdIfExeCmdReply.job_id = m_jobId;
	rep->cmdIfExeCmdReply.result = result;
	std::strcpy(rep->cmdIfExeCmdReply.output, m_output.str().c_str());

	// TPT_TRACE(TRACE_DEBUG, SSTR("rep = 0x", std::hex, rep));
	// TPT_TRACE(TRACE_DEBUG, SSTR("job_id = 0x", std::hex, &(rep->cmdIfExeCmdReply.job_id)));
	// TPT_TRACE(TRACE_DEBUG, SSTR("result = 0x", std::hex, &(rep->cmdIfExeCmdReply.result)));
	// TPT_TRACE(TRACE_DEBUG, SSTR("output = 0x", std::hex, (unsigned long long)(rep->cmdIfExeCmdReply.output)));

	if(!itc_send(&rep, m_clidMboxId, ITC_MY_MBOX_ID, NULL))
	{
		TPT_TRACE(TRACE_ERROR, SSTR("Failed to send CMDIF_EXE_CMD_REPLY to clid for cmdName = \"", m_cmdName, "\""));
		return;
	}

	TPT_TRACE(TRACE_INFO, SSTR("Send CMDIF_EXE_CMD_REPLY to clid successfully!"));
}

} // V1

} // namespace CmdIf