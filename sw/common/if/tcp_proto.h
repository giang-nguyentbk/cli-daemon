/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#ifndef __TCP_PROTO_H__
#define __TCP_PROTO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <netinet/in.h>


#define CLID_PAYLOAD_TYPE_BASE		0x10000

#define CLID_GET_LIST_CMD_REQUEST	(CLID_PAYLOAD_TYPE_BASE + 0x1)
struct clid_get_list_cmd_request {
	// uint32_t	payload_startpoint;
	uint32_t	errorcode;
};

#define CLID_GET_LIST_CMD_REPLY		(CLID_PAYLOAD_TYPE_BASE + 0x2)
struct clid_get_list_cmd_reply {
	// uint32_t	payload_startpoint;
	uint32_t	errorcode;
	uint32_t	payload_length;
	char		payload[1];
	/* Format:
	+ payload_length = number of bytes that payload has
	+ payload:
		+ num_cmds: first two bytes (uint16_t): number of cmds

		+ cmd_len1: two bytes (uint16_t): number of bytes that the 1st cmd has
		+ cmd1: "cmd_len1" bytes in form of string (not include '\0')
		+ cmd_desc_len1: two bytes (uint16_t): number of bytes that describes what the 1st cmd does
		+ cmd_desc1: bytes in form of string (not include '\0')

		+ cmd_len2: two bytes (uint16_t): number of bytes that the 2nd cmd has
		+ cmd2: "cmd_len2" bytes in form of string (not include '\0')
		+ cmd_desc_len2: two bytes (uint16_t): number of bytes that describes what the 2nd cmd does
		+ cmd_desc2: bytes in form of string (not include '\0')

		...
		...

		+ cmd_lenn: two bytes (uint16_t): number of bytes that the n-th cmd has
		+ cmdn: "cmd_lenn" bytes in form of string (not include '\0')
		+ cmd_desc_lenn: two bytes (uint16_t): number of bytes that describes what the n-th cmd does
		+ cmd_descn: bytes in form of string (not include '\0')
	*/
};

#define CLID_EXE_CMD_REQUEST		(CLID_PAYLOAD_TYPE_BASE + 0x3)
struct clid_exe_cmd_request {
	// uint32_t	payload_startpoint;
	uint32_t	errorcode;
	uint32_t	payload_length;
	char		payload[1];
	/* Format:
	+ payload_length = number of bytes that payload has
	+ payload:
		+ cmd_name_len: first two bytes (uint16_t): cmd_name length
		+ cmd_name: "cmd_name_len" bytes in form of string (not include '\0')

		+ num_args: two bytes (uint16_t): number of arguments

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

#define CLID_EXE_CMD_REPLY		(CLID_PAYLOAD_TYPE_BASE + 0x4)
struct clid_exe_cmd_reply {
	// uint32_t	payload_startpoint;
	uint32_t	errorcode;
	uint32_t	payload_length;
	char		payload[1]; // String that shell client will print out for the user about results of the requested command
};


typedef enum {
	CLID_STATUS_OK = 0,
	CLID_INVALID_TYPE,
	CLID_NUM_OF_STATUS
} status_e;

struct ethtcp_header {
	uint32_t	sender;
	uint32_t	receiver;
	uint32_t	protRev;
	uint32_t	msgno;
	uint32_t	payloadLen;
};

struct ethtcp_msg {
	struct ethtcp_header					header;
	union {
		// uint32_t					payload_startpoint;
		uint32_t					errorcode;
		struct clid_get_list_cmd_request		clid_get_list_cmd_request;
		struct clid_get_list_cmd_reply			clid_get_list_cmd_reply;
		struct clid_exe_cmd_request			clid_exe_cmd_request;
		struct clid_exe_cmd_reply			clid_exe_cmd_reply;
	} payload;
};

#ifdef __cplusplus
}
#endif

#endif // __TCP_PROTO_H__