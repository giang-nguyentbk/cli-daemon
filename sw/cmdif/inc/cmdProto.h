/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#pragma once

#include <stdint.h> // Suitable for both C and C++. Intead of that, cstdint only possible until C++11 and not available in C, so would fail if clid.c include this header.
#include <itc.h>

#define MAX_CMD_NAME_LENGTH	32

#define CMDIF_RET_SUCCESS				10
#define CMDIF_RET_INVALID_ARGS				20
#define CMDIF_RET_FAIL					30

#define CMDIF_MSGBASE					0x17700000
#define CMDIF_REG_CMD_REQUEST				(CMDIF_MSGBASE + 1)
#define CMDIF_DEREG_CMD_REQUEST				(CMDIF_MSGBASE + 2)
#define CMDIF_EXE_CMD_REQUEST				(CMDIF_MSGBASE + 3)
#define CMDIF_EXE_CMD_REPLY				(CMDIF_MSGBASE + 4)


struct CmdIfRegCmdRequestS
{
	uint32_t msgno;
	itc_mbox_id_t mailbox_id;
	char cmd_name[MAX_CMD_NAME_LENGTH];
	char cmd_desc[1];
};

struct CmdIfDeregCmdRequestS
{
	uint32_t msgno;
	uint8_t cmd_name[1];
};

struct CmdIfExeCmdRequestS
{
	uint32_t msgno;
	uint32_t payloadLen;
	uint8_t payload[1];

};

struct CmdIfExeCmdReplyS
{
	uint32_t msgno;
	uint32_t result;
	uint32_t payloadLen;
	uint8_t payload[1];

};


union itc_msg
{
	uint32_t					msgno;
	struct CmdIfRegCmdRequestS			cmdIfRegCmdRequest;
	struct CmdIfDeregCmdRequestS			cmdIfDeregCmdRequest;
	struct CmdIfExeCmdRequestS			cmdIfExeCmdRequest;
	struct CmdIfExeCmdReplyS			cmdIfExeCmdReply;
};