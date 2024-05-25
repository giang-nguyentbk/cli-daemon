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

#include "cmdTypesIf.h"

namespace CmdIf
{

namespace V1
{

class CmdJobIf
{
public:
	virtual const std::string& getCmdName() const = 0;
	virtual const std::vector<std::string>& getArgs() const = 0;
	virtual std::string& getOutputStream() = 0;
	virtual void done(const CmdIf::V1::CmdTypesIf::CmdResultCode& rc) = 0;

	// Avoid copy/move constructors, assigments
	CmdJobIf(const CmdJobIf&) 		= delete;
	CmdJobIf(CmdJobIf&&) 			= delete;
	CmdJobIf& operator=(const CmdJobIf&) 	= delete;
	CmdJobIf& operator=(CmdJobIf&&) 	= delete;

protected:
	CmdJobIf() = default;
	~CmdJobIf() = default;

}; // class CmdJobIf

} // V1

} // namespace CmdIf