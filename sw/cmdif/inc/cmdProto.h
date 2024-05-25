/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#pragma once

#include <cstdint>
#include <itc.h>

#define CMDIF_RET_SUCCESS				10
#define CMDIF_RET_INVALID_ARGS				20
#define CMDIF_RET_FAIL					30

#define CMDIF_MSGBASE					0x17700000
#define CMDIF_REG_CMD_REQUEST				(CMDIF_MSGBASE + 1)
#define CMDIF_REG_CMD_REPLY				(CMDIF_MSGBASE + 2)
#define CMDIF_DEREG_CMD_REQUEST				(CMDIF_MSGBASE + 3)
#define CMDIF_DEREG_CMD_REPLY				(CMDIF_MSGBASE + 4)
#define CMDIF_EXE_CMD_REQUEST				(CMDIF_MSGBASE + 5)
#define CMDIF_EXE_CMD_REPLY				(CMDIF_MSGBASE + 6)


struct CmdIfRegCmdRequestS
{
	uint32_t msgNo;
	uint32_t payloadLen;
	uint8_t payload[1];

	/* Format:
	
	
	*/
};

struct CmdIfRegCmdReplyS
{
	uint32_t msgNo;
};

struct CmdIfDeregCmdRequestS
{
	uint32_t msgNo;
	uint32_t payloadLen;
	uint8_t payload[1];

	/* Format:
	
	
	*/
};

struct CmdIfDeregCmdReplyS
{
	uint32_t msgNo;
};

struct CmdIfExeCmdRequestS
{
	uint32_t msgNo;
	uint32_t payloadLen;
	uint8_t payload[1];

	/* Format:
	
	
	*/
};

struct CmdIfExeCmdReplyS
{
	uint32_t msgNo;
	uint32_t result;
	uint32_t payloadLen;
	uint8_t payload[1];

	/* Format:
	
	
	*/
};


union itc_msg
{
	struct CmdIfRegCmdRequestS			cmdIfRegCmdRequest;
	struct CmdIfRegCmdReplyS			cmdIfRegCmdReply;
	struct CmdIfDeregCmdRequestS			cmdIfDeregCmdRequest;
	struct CmdIfDeregCmdReplyS			cmdIfDeregCmdReply;
	struct CmdIfExeCmdRequestS			cmdIfExeCmdRequest;
	struct CmdIfExeCmdReplyS			cmdIfExeCmdReply;
};