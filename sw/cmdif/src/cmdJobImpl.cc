#include <vector>
#include <string>
#include <cstring>

#include <itc.h>
#include <traceIf.h>

#include "cmdJobImpl.h"
#include "cmdTypesIf.h"
#include "cmdProto.h"


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
		TPT_TRACE(TRACE_ERROR, "Unknown CmdResultCode rc = %d!", rc);
		return;
	}

	uint32_t len = m_output.str().length();
	union itc_msg* rep = itc_alloc(offsetof(struct CmdIfExeCmdReplyS, output) + len + 1, CMDIF_EXE_CMD_REPLY);
	rep->cmdIfExeCmdReply.job_id = m_jobId;
	rep->cmdIfExeCmdReply.result = result;
	std::strcpy(rep->cmdIfExeCmdReply.output, m_output.str().c_str());

	// TPT_TRACE(TRACE_DEBUG, "rep = 0x%016x", rep);
	// TPT_TRACE(TRACE_DEBUG, "job_id = 0x%016x", &(rep->cmdIfExeCmdReply.job_id));
	// TPT_TRACE(TRACE_DEBUG, "result = 0x%016x", &(rep->cmdIfExeCmdReply.result));
	// TPT_TRACE(TRACE_DEBUG, "output = 0x%016x", (unsigned long long)(rep->cmdIfExeCmdReply.output));

	if(!itc_send(&rep, m_clidMboxId, ITC_MY_MBOX_ID, NULL))
	{
		TPT_TRACE(TRACE_ERROR, "Failed to send CMDIF_EXE_CMD_REPLY to clid for cmdName = %s!", m_cmdName);
		return;
	}

	TPT_TRACE(TRACE_INFO, "Send CMDIF_EXE_CMD_REPLY to clid successfully!");
}

} // V1

} // namespace CmdIf