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
#include <string>
#include <itc.h>

#include "cmdJobIf.h"

namespace CmdIf
{

namespace V1
{

class CmdJobImpl : public CmdJobIf
{
public:
	CmdJobImpl(const std::string& cmdName, const std::vector<std::string>& args, std::string& output, itc_mbox_id_t clidMboxId);
	virtual ~CmdJobImpl();

	const std::string& getCmdName() const override
	{
		return m_cmdName;
	}

	const std::vector<std::string>& getArgs() const override
	{
		return m_args;
	}

	std::string& getOutputStream() override
	{
		return m_output;
	}

	void done(const CmdIf::V1::CmdTypesIf::CmdResultCode& rc) override;

	// Avoid copy/move constructors, assigments
	CmdJobImpl(const CmdJobImpl&) 			= delete;
	CmdJobImpl(CmdJobImpl&&) 			= delete;
	CmdJobImpl& operator=(const CmdJobImpl&) 	= delete;
	CmdJobImpl& operator=(CmdJobImpl&&) 		= delete;

private:
	unsigned long long m_jobId;
	std::string m_cmdName;
	std::vector<std::string> m_args;
	std::string m_output;
	itc_mbox_id_t m_clidMboxId;

}; // class CmdJobImpl

} // V1

} // namespace CmdIf