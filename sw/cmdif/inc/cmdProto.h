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
	char cmd_name[MAX_CMD_NAME_LENGTH];
	char cmd_desc[1];
};

struct CmdIfDeregCmdRequestS
{
	uint32_t msgno;
	char cmd_name[1];
};

struct CmdIfExeCmdRequestS
{
	uint32_t msgno;
	unsigned long long job_id;
	char cmd_name[MAX_CMD_NAME_LENGTH];
	uint32_t num_args;
	uint32_t payloadLen;
	char payload[1];

	/*
	Payload format:
		+ arg_len1: two bytes (uint16_t): number of bytes that the 1st argument has
		+ arg1: "arg_len1" bytes in form of string (not include '\0')

		+ arg_len2: two bytes (uint16_t): number of bytes that the 2nd argument has
		+ arg2: "arg_len2" bytes in form of string (not include '\0')

		...
		...

		+ arg_lenn: two bytes (uint16_t): number of bytes that the n-th argument has
		+ argn: "arg_lenn" bytes in form of string (not include '\0')
	*/

};

struct CmdIfExeCmdReplyS
{
	uint32_t msgno;
	unsigned long long job_id;
	uint32_t result;
	char output[1];
};


union itc_msg
{
	uint32_t					msgno;
	struct CmdIfRegCmdRequestS			cmdIfRegCmdRequest;
	struct CmdIfDeregCmdRequestS			cmdIfDeregCmdRequest;
	struct CmdIfExeCmdRequestS			cmdIfExeCmdRequest;
	struct CmdIfExeCmdReplyS			cmdIfExeCmdReply;
};