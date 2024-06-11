#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/timerfd.h>
#include <search.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stddef.h>

#include <itc.h>
#include <traceIf.h>

#include "tcp_proto.h"

/*****************************************************************************\/
*****                          INTERNAL TYPES                              *****
*******************************************************************************/
#define MAX_OF(a, b)		(a) > (b) ? (a) : (b)
#define MIN_OF(a, b)		(a) < (b) ? (a) : (b)
#define TCP_CLID_PORT		33333
#define MAX_NUM_SHELL_CLIENTS	255
#define MAX_NUM_CMDS		255
#define MAX_SUB_CMD_NAME_LENGTH	255
#define MAX_CMD_DESC_LENGTH	128
#define MAX_CMD_NAME_LENGTH	32
#define MAX_NUM_SUB_CMDS	64
#define NET_INTERFACE_ETH0	"eth0"
#define CLID_LOG_FILENAME	"clid.log"
#define CLID_MBOX_NAME		"clid_mailbox"


struct shell_client {
	int		fd;

};

struct sub_command {
	char		sub_cmd[MAX_SUB_CMD_NAME_LENGTH];
};

struct command {
	char			cmd_name[MAX_CMD_NAME_LENGTH];
	char			cmd_desc[MAX_CMD_DESC_LENGTH];
	struct sub_command	sub_cmds[MAX_NUM_SUB_CMDS];
};

struct clid_instance {
	int					tcp_fd;
	struct sockaddr_in			tcp_addr;
	struct shell_client			clients[MAX_NUM_SHELL_CLIENTS];
	void					*client_tree;
	uint16_t				cmd_count;
	struct command				cmds[MAX_NUM_CMDS];
	void					*cmd_tree;
	int					mbox_fd;
	itc_mbox_id_t				mbox_id;
};


/*****************************************************************************\/
*****                          INTERNAL VARIABLES                          *****
*******************************************************************************/
static struct clid_instance clid_inst;



/*****************************************************************************\/
*****                     INTERNAL FUNCTIONS PROTOTYPES                    *****
*******************************************************************************/
static void clid_init(void);
static void clid_sig_handler(int signo);
static void clid_exit_handler(void);
static bool setup_log_file(void);
static bool setup_mailbox(void);
static bool setup_tcp_server(void);
static struct in_addr get_ip_address_from_network_interface(int sockfd, char *interface);
static bool handle_accept_new_connection(int sockfd);
static int compare_fd_in_client_tree(const void *pa, const void *pb);
static int compare_client_in_client_tree(const void *pa, const void *pb);
static bool setup_shell_clients(void);
static bool setup_command_list(void);
static bool handle_receive_tcp_packet(int sockfd);
static int recv_data(int sockfd, void *rx_buff, int nr_bytes_to_read);
static bool release_shell_client_resources(int sockfd);
static bool handle_receive_get_list_cmd_request(int sockfd, struct ethtcp_header *header);
static bool handle_receive_exe_cmd_request(int sockfd, struct ethtcp_header *header);
static void do_nothing(void *tree_node_data);
static bool send_get_list_cmd_reply(int sockfd);
static bool send_exe_cmd_reply(int sockfd);



/*****************************************************************************\/
*****                             MAIN FUNCTION                            *****
*******************************************************************************/
int main(int argc, char* argv[])
{
	clid_init();

	int opt = 0;
	bool is_daemon = false;

	while((opt = getopt(argc, argv, "d")) != -1)
	{
		switch (opt)
		{
		case 'd':
			is_daemon = true;
			break;
		
		default:
			printf("ERROR: Usage:\t%s\t[-d]\n", argv[0]);
			printf("Example:\t%s\t-d\n", argv[0]);
			printf("=> This will start clid as a daemon!\n");
			exit(EXIT_FAILURE);
			break;
		}
	}

	if(is_daemon)
	{
		printf(">>> Starting clid daemon...\n");

		if(!setup_log_file())
		{
			printf("Failed to setup log file for this clid daemon!\n");
			exit(EXIT_FAILURE);
		}

		if(daemon(1, 1))
		{
			printf("Failed to start clid as a daemon!\n");
			exit(EXIT_FAILURE);
		}

		printf("Starting clid daemon...\n");
	} else
	{
		printf("Starting clid, but not as a daemon...\n");
	}

	// At normal termination we just clean up our resources by registration a exit_handler
	atexit(clid_exit_handler);

	if(!setup_tcp_server() || !setup_shell_clients() || !setup_command_list() || !setup_mailbox())
	{
		TPT_TRACE(TRACE_ERROR, "Failed to setup clid daemon!");
		exit(EXIT_FAILURE);
	}

	fd_set fdset;
	int max_fd = -1;
	int res = 0;
	while(1)
	{
		FD_ZERO(&fdset);
		FD_SET(clid_inst.tcp_fd, &fdset);
		max_fd = MAX_OF(clid_inst.tcp_fd, max_fd);

		for(int i = 0; i < MAX_NUM_SHELL_CLIENTS; i++)
		{
			if(clid_inst.clients[i].fd != -1)
			{
				FD_SET(clid_inst.clients[i].fd, &fdset);
				max_fd = MAX_OF(clid_inst.clients[i].fd, max_fd);
			}
		}

		res = select(max_fd + 1, &fdset, NULL, NULL, NULL);
		if(res < 0)
		{
			TPT_TRACE(TRACE_ERROR, "Failed to select()!");
			exit(EXIT_FAILURE);
		}

		if(FD_ISSET(clid_inst.tcp_fd, &fdset))
		{
			if(handle_accept_new_connection(clid_inst.tcp_fd) == false)
			{
				TPT_TRACE(TRACE_ERROR, "Failed to handle_accept_new_connection()!");
				exit(EXIT_FAILURE);
			}
		}

		for(int i = 0; i < MAX_NUM_SHELL_CLIENTS; i++)
		{
			if(clid_inst.clients[i].fd != -1 && FD_ISSET(clid_inst.clients[i].fd, &fdset))
			{
				if(handle_receive_tcp_packet(clid_inst.clients[i].fd) == false)
				{
					TPT_TRACE(TRACE_ERROR, "Failed to handle_accept_new_connection()!");
					exit(EXIT_FAILURE);
				}
			}
		}
	}

}


/*****************************************************************************\/
*****                  INTERNAL FUNCTIONS IMPLEMENTATION                   *****
*******************************************************************************/
static void clid_init(void)
{
	/* Ignore SIGPIPE signal, because by any reason, any socket/fd that was connected
	** to this process is corrupted a SIGPIPE will be sent to this process and causes it crash.
	** By ignoring this signal, clid can be run as a daemon (run on background) */
	signal(SIGPIPE, SIG_IGN);
	// Call our own exit_handler to release all resources if receiving any of below signals
	signal(SIGSEGV, clid_sig_handler);
	signal(SIGILL, clid_sig_handler); // When CPU executed an instruction it did not understand
	signal(SIGABRT, clid_sig_handler);
	signal(SIGFPE, clid_sig_handler); // Reports a fatal arithmetic error, for example divide-by-zero
	signal(SIGTERM, clid_sig_handler);
	signal(SIGINT, clid_sig_handler);
}

static void clid_sig_handler(int signo)
{
	// Call our own exit_handler
	TPT_TRACE(TRACE_INFO, "CLID is terminated with SIG = %d, calling exit handler...", signo);
	clid_exit_handler();

	// After clean up, resume raising the suppressed signal
	signal(signo, SIG_DFL); // Inform kernel does fault exit_handler for this kind of signal
	raise(signo);
}

static void clid_exit_handler(void)
{
	TPT_TRACE(TRACE_INFO, "CLID is terminated, calling exit handler...");

	close(clid_inst.tcp_fd);
	tdestroy(clid_inst.client_tree, do_nothing);
	
	TPT_TRACE(TRACE_INFO, "CLID exit handler finished!");
}

static bool setup_log_file(void)
{
	/* Setup a log file for our itcgws daemon */
	freopen(CLID_LOG_FILENAME, "a+", stdout);
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stderr);

	fprintf(stdout, "========================================================================================================================\n");
	fflush(stdout);
	fprintf(stdout, ">>>>>>>                                             START NEW SESSION                                            <<<<<<<\n");
	fflush(stdout);
	fprintf(stdout, "========================================================================================================================\n");
	fflush(stdout);

	return true;
}

static bool setup_mailbox(void)
{
	if(itc_init(3, ITC_MALLOC, 0) == false)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to itc_init() by CLID!");
		return false;
	}

	clid_inst.mbox_id = itc_create_mailbox(CLID_MBOX_NAME, ITC_NO_NAMESPACE);
	if(clid_inst.mbox_id == ITC_NO_MBOX_ID)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to create mailbox %s", CLID_MBOX_NAME);
		return false;
	}

	clid_inst.mbox_fd = itc_get_fd(clid_inst.mbox_id);
	TPT_TRACE(TRACE_INFO, "Create TCP server mailbox \"%s\" successfully!", CLID_MBOX_NAME);
	return true;
}

static bool setup_tcp_server(void)
{
	int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
	if(tcpfd < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to get socket(), errno = %d!", errno);
		return false;
	}

	int listening_opt = 1;
	int res = setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &listening_opt, sizeof(int));
	if(res < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to set sockopt SO_REUSEADDR, errno = %d!", errno);
		close(tcpfd);
		return false;
	}

	memset(&clid_inst.tcp_addr, 0, sizeof(struct sockaddr_in));
	size_t size = sizeof(struct sockaddr_in);
	clid_inst.tcp_addr.sin_family = AF_INET;
	clid_inst.tcp_addr.sin_addr = get_ip_address_from_network_interface(tcpfd, NET_INTERFACE_ETH0);
	clid_inst.tcp_addr.sin_port = htons(TCP_CLID_PORT);

	res = bind(tcpfd, (struct sockaddr *)&clid_inst.tcp_addr, size);
	if(res < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to bind, errno = %d!", errno);
		close(tcpfd);
		return false;
	}

	res = listen(tcpfd, MAX_NUM_SHELL_CLIENTS);
	if(res < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to listen, errno = %d!", errno);
		close(tcpfd);
		return false;
	}

	clid_inst.tcp_fd = tcpfd;

	TPT_TRACE(TRACE_INFO, "Setup TCP server successfully on %s:%d", inet_ntoa(clid_inst.tcp_addr.sin_addr), ntohs(clid_inst.tcp_addr.sin_port));
	return true;
}

static struct in_addr get_ip_address_from_network_interface(int sockfd, char *interface)
{
	struct sockaddr_in sock_addr;
	struct ifreq ifrq;
	memset(&ifrq, 0, sizeof(struct ifreq));
	int size = strlen(interface) + 1;
	memcpy(&(ifrq.ifr_ifrn.ifrn_name), interface, size);

	/* Get IP address from network interface, such as: lo, eth0, eth1,... */
	size = sizeof(struct ifreq);
	int res = ioctl(sockfd, SIOCGIFADDR, (caddr_t)&ifrq, size);
	if(res < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to ioctl to obtain IP address from %s, errno = %d!", interface, errno);
		return sock_addr.sin_addr;
	}

	size = sizeof(struct sockaddr_in);
	memcpy(&sock_addr, &(ifrq.ifr_ifru.ifru_addr), size);

	TPT_TRACE(TRACE_INFO, "Retrieve address from network interface \"%s\" -> tcp://%s:%d", interface, inet_ntoa(sock_addr.sin_addr), sock_addr.sin_port);
	return sock_addr.sin_addr;
}

static bool handle_accept_new_connection(int sockfd)
{
	struct sockaddr_in new_addr;
	unsigned int addr_size = sizeof(struct sockaddr_in);
	memset(&new_addr, 0, addr_size);

	int new_fd = accept(sockfd, (struct sockaddr *)&new_addr, (socklen_t*)&addr_size);
	if(new_fd < 0)
	{
		if(errno == EINTR)
		{
			TPT_TRACE(TRACE_ABN, "Accepting connection was interrupted, just ignore it!");
			return true;
		} else
		{
			TPT_TRACE(TRACE_ERROR, "Accepting connection was destroyed!");
			return false;
		}
	}

	TPT_TRACE(TRACE_INFO, "Receiving new connection from a peer client tcp://%s:%hu/", inet_ntoa(new_addr.sin_addr), ntohs(new_addr.sin_port));

	struct shell_client **iter;
	iter = tfind(&new_fd, &clid_inst.client_tree, compare_fd_in_client_tree);
	if(iter != NULL)
	{
		/* Already added in tree */
		TPT_TRACE(TRACE_ABN, "This fd %d already connected, something wrong!", new_fd);
		return false;
	} else
	{
		int i = 0;
		for(; i < MAX_NUM_SHELL_CLIENTS; i++)
		{
			if(clid_inst.clients[i].fd == -1)
			{
				clid_inst.clients[i].fd = new_fd;
				tsearch(&clid_inst.clients[i], &clid_inst.client_tree, compare_client_in_client_tree);
				break;
			}
		}

		if(i == MAX_NUM_SHELL_CLIENTS)
		{
			TPT_TRACE(TRACE_ERROR, "No more than %d shell client is accepted!", MAX_NUM_SHELL_CLIENTS);
			return false;
		}
	}

	return true;
}

static int compare_fd_in_client_tree(const void *pa, const void *pb)
{
	const int *fd = pa;
	const struct shell_client *client = pb;

	if(*fd == client->fd)
	{
		return 0;
	} else if(*fd > client->fd)
	{
		return 1;
	} else
	{
		return -1;
	}
}

static int compare_client_in_client_tree(const void *pa, const void *pb)
{
	const struct shell_client *client_a = pa;
	const struct shell_client *client_b = pb;
	
	if(client_a->fd == client_b->fd)
	{
		return 0;
	} else if(client_a->fd > client_b->fd)
	{
		return 1;
	} else
	{
		return -1;
	}
}

static bool setup_shell_clients(void)
{
	for(int i = 0; i < MAX_NUM_SHELL_CLIENTS; i++)
	{
		clid_inst.clients[i].fd = -1;
	}

	return true;
}

static bool setup_command_list(void)
{
	clid_inst.cmd_count = 0;

	strcpy(clid_inst.cmds[0].cmd_name, "aclocal");
	strcpy(clid_inst.cmds[0].cmd_desc, "Run aclocal command.");
	strcpy(clid_inst.cmds[0].sub_cmds[0].sub_cmd, "aclocal 1 2 3");
	strcpy(clid_inst.cmds[0].sub_cmds[1].sub_cmd, "aclocal a b c");
	for(int i = 2 ; i < MAX_NUM_SUB_CMDS; i++)
	{
		clid_inst.cmds[0].sub_cmds[i].sub_cmd[0] = '\0';
	}
	clid_inst.cmd_count++;

	strcpy(clid_inst.cmds[1].cmd_name, "acreconf");
	strcpy(clid_inst.cmds[1].cmd_desc, "Run acreconf command.");
	strcpy(clid_inst.cmds[1].sub_cmds[0].sub_cmd, "acreconf 1 2 3");
	strcpy(clid_inst.cmds[1].sub_cmds[1].sub_cmd, "acreconf a b c");
	for(int i = 2 ; i < MAX_NUM_SUB_CMDS; i++)
	{
		clid_inst.cmds[1].sub_cmds[i].sub_cmd[0] = '\0';
	}
	clid_inst.cmd_count++;

	strcpy(clid_inst.cmds[2].cmd_name, "aclog");
	strcpy(clid_inst.cmds[2].cmd_desc, "Run aclog command.");
	strcpy(clid_inst.cmds[2].sub_cmds[0].sub_cmd, "aclog 1 2 3");
	strcpy(clid_inst.cmds[2].sub_cmds[1].sub_cmd, "aclog a b c");
	for(int i = 2 ; i < MAX_NUM_SUB_CMDS; i++)
	{
		clid_inst.cmds[2].sub_cmds[i].sub_cmd[0] = '\0';
	}
	clid_inst.cmd_count++;

	for(int i = 3; i < MAX_NUM_CMDS; i++)
	{
		clid_inst.cmds[i].cmd_name[0] = '\0';
		for(int j = 0 ; j < MAX_NUM_SUB_CMDS; j++)
		{
			clid_inst.cmds[i].sub_cmds[j].sub_cmd[0] = '\0';
		}
	}

	return true;
}

static bool handle_receive_tcp_packet(int sockfd)
{
	struct ethtcp_header *header;
	int header_size = sizeof(struct ethtcp_header);
	char rxbuff[header_size];
	int size = 0;

	size = recv_data(sockfd, rxbuff, header_size);

	if(size == 0)
	{
		TPT_TRACE(TRACE_INFO, "Shell client from this fd %d just disconnected, remove it from our client list!", sockfd);
		if(!release_shell_client_resources(sockfd))
		{
			TPT_TRACE(TRACE_ERROR, "Failed to release_shell_client_resources()!");
		}

		return true;
	} else if(size < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Receive data from this shell client failed, fd = %d!", sockfd);
		return false;
	}

	header = (struct ethtcp_header *)rxbuff;
	header->msgno 			= ntohl(header->msgno);
	header->payloadLen 		= ntohl(header->payloadLen);
	header->protRev			= ntohl(header->protRev);
	header->receiver		= ntohl(header->receiver);
	header->sender			= ntohl(header->sender);

	TPT_TRACE(TRACE_INFO, "Receiving %d bytes from fd %d", size, sockfd);
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: msgno: 0x%08x", header->msgno);
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: payloadLen: %u", header->payloadLen);
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: protRev: %u", header->protRev);
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: receiver: %u", header->receiver);
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: sender: %u", header->sender);

	switch (header->msgno)
	{
	case CLID_GET_LIST_CMD_REQUEST:
		TPT_TRACE(TRACE_INFO, "Received CLID_GET_LIST_CMD_REQUEST!");
		handle_receive_get_list_cmd_request(sockfd, header);
		break;
	
	case CLID_EXE_CMD_REQUEST:
		TPT_TRACE(TRACE_INFO, "Received CLID_GET_LIST_CMD_REQUEST!");
		handle_receive_exe_cmd_request(sockfd, header);
		break;
	
	default:
		TPT_TRACE(TRACE_ABN, "Received unknown TCP packet, drop it!");
		break;
	}

	return true;
}

static int recv_data(int sockfd, void *rx_buff, int nr_bytes_to_read)
{
	int length = 0;
	int read_count = 0;

	do
	{
		length = recv(sockfd, (char *)rx_buff + read_count, nr_bytes_to_read, 0);
		if(length <= 0)
		{
			return length;
		}

		read_count += length;
		nr_bytes_to_read = nr_bytes_to_read - length;
	} while(nr_bytes_to_read > 0);

	return read_count;
}

static bool release_shell_client_resources(int sockfd)
{
	int i = 0;
	for(; i < MAX_NUM_SHELL_CLIENTS; i++)
	{
		if(clid_inst.clients[i].fd == sockfd)
		{
			close(clid_inst.clients[i].fd);
			clid_inst.clients[i].fd = -1;

			struct shell_client **iter;
			iter = tfind(&clid_inst.clients[i].fd, &clid_inst.client_tree, compare_fd_in_client_tree);
			if(iter == NULL)
			{
				TPT_TRACE(TRACE_ABN, "Disconnected shell client not found in the tree, something wrong!");
				return false;
			}

			tdelete(*iter, &clid_inst.client_tree, compare_client_in_client_tree);

			break;
		}
	}

	if(i == MAX_NUM_SHELL_CLIENTS)
	{
		TPT_TRACE(TRACE_ABN, "Disconnected peer not found in server list, something wrong!");
		return false;
	}

	return true;
}

static bool handle_receive_get_list_cmd_request(int sockfd, struct ethtcp_header *header)
{
	struct clid_get_list_cmd_request *req;
	uint32_t payloadLen = header->payloadLen;
	char rxbuff[payloadLen];
	int size = 0;

	size = recv_data(sockfd, rxbuff, payloadLen);

	if(size <= 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to receive data from this shell client, fd = %d!", sockfd);
		return false;
	}

	req = (struct clid_get_list_cmd_request *)rxbuff;
	req->errorcode = ntohl(req->errorcode);

	TPT_TRACE(TRACE_INFO, "Receiving %d bytes from fd %d", size, sockfd);
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: errorcode: %u", req->errorcode);

	if(!send_get_list_cmd_reply(sockfd))
	{
		return false;
	}
	
	return true;
}

static bool handle_receive_exe_cmd_request(int sockfd, struct ethtcp_header *header)
{
	struct clid_exe_cmd_request *req;
	uint32_t payloadLen = header->payloadLen;
	char rxbuff[payloadLen];
	int size = 0;

	size = recv_data(sockfd, rxbuff, payloadLen);

	if(size <= 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to receive data from this shell client, fd = %d!", sockfd);
		return false;
	}

	req = (struct clid_exe_cmd_request *)rxbuff;
	req->errorcode = ntohl(req->errorcode);
	req->timeout = ntohl(req->timeout);
	req->payload_length = ntohl(req->payload_length);

	TPT_TRACE(TRACE_INFO, "Receiving %d bytes from fd %d", size, sockfd);
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: errorcode: %u", req->errorcode);
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: timeout: %u", req->timeout);
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: payload_length: %u", req->payload_length);

	unsigned long offset = 0;
	uint16_t len = 0;
	char buff[MAX_CMD_NAME_LENGTH];

	len = *((uint16_t *)(&req->payload + offset));
	offset += 2;
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: cmd_name_len: %hu", len);
	memcpy(buff, (&req->payload + offset), len);
	buff[len] = '\0';
	offset += len;
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: cmd_name: %s", buff);

	uint16_t num_args = *((uint16_t *)(&req->payload + offset));
	offset += 2;
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: num_args: %hu", num_args);

	for(int i = 0; i < num_args; i++)
	{
		/* This is arguments */
		len = *((uint16_t *)(&req->payload + offset));
		offset += 2;
		memcpy(buff, (&req->payload + offset), len);
		buff[len] = '\0';
		offset += len;
		TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: arg_len %d: %hu", i, len);
		TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: arg %d: %s", i, buff);
	}

	// TODO: forward to respective mailbox who registered cmds 

	if(!send_exe_cmd_reply(sockfd))
	{
		return false;
	}
	
	return true;
}

static void do_nothing(void *tree_node_data)
{
	(void)tree_node_data;
}

static bool send_get_list_cmd_reply(int sockfd)
{
	uint32_t total_len = 2; // First two bytes for number of cmds
	uint16_t cmd_len = 0;
	char cmds_buff[2 + clid_inst.cmd_count*(2 + MAX_CMD_NAME_LENGTH + 2 + MAX_CMD_DESC_LENGTH)];

	for(int i = 0; i < MAX_NUM_CMDS; i++)
	{
		if(clid_inst.cmds[i].cmd_name[0] != '\0')
		{
			cmd_len = strlen(clid_inst.cmds[i].cmd_name);
			*((uint16_t *)(&cmds_buff[total_len])) = cmd_len;
			total_len += 2;
			strcpy(&cmds_buff[total_len], clid_inst.cmds[i].cmd_name);
			total_len += cmd_len;

			cmd_len = strlen(clid_inst.cmds[i].cmd_desc);
			*((uint16_t *)(&cmds_buff[total_len])) = cmd_len;
			total_len += 2;
			strcpy(&cmds_buff[total_len], clid_inst.cmds[i].cmd_desc);
			total_len += cmd_len;
		}
	}

	*((uint16_t *)(&cmds_buff[0])) = clid_inst.cmd_count;

	size_t msg_len = offsetof(struct ethtcp_msg, payload) + offsetof(struct clid_get_list_cmd_reply, payload) + total_len;
	struct ethtcp_msg *rep = malloc(msg_len);
	if(rep == NULL)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to malloc locate mbox reply message!");
		return false;
	}

	uint32_t payload_length = offsetof(struct clid_get_list_cmd_reply, payload) + total_len;
	rep->header.sender 					= htonl((uint32_t)getpid());
	rep->header.receiver 					= htonl(111);
	rep->header.protRev 					= htonl(15);
	rep->header.msgno 					= htonl(CLID_GET_LIST_CMD_REPLY);
	rep->header.payloadLen 					= htonl(payload_length);

	rep->payload.clid_get_list_cmd_reply.errorcode		= htonl(CLID_STATUS_OK);
	rep->payload.clid_get_list_cmd_reply.payload_length	= htonl(total_len);
	memcpy(rep->payload.clid_get_list_cmd_reply.payload, cmds_buff, total_len);

	int res = send(sockfd, rep, msg_len, 0);
	if(res < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to send CLID_GET_LIST_CMD_REPLY, errno = %d!", errno);
		return false;
	}

	free(rep);
	TPT_TRACE(TRACE_INFO, "Sent CLID_GET_LIST_CMD_REPLY successfully!");
	return true;
}

static bool send_exe_cmd_reply(int sockfd)
{
	char cmd_output[512];
	strcpy(cmd_output, "This is a notification toward user notifying that the command was executing successfully!");
	uint32_t total_len = strlen(cmd_output);

	size_t msg_len = offsetof(struct ethtcp_msg, payload) + offsetof(struct clid_exe_cmd_reply, payload) + total_len;
	struct ethtcp_msg *rep = malloc(msg_len);
	if(rep == NULL)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to malloc locate mbox reply message!");
		return false;
	}

	uint32_t payload_length = offsetof(struct clid_exe_cmd_reply, payload) + total_len;
	rep->header.sender 					= htonl((uint32_t)getpid());
	rep->header.receiver 					= htonl(111);
	rep->header.protRev 					= htonl(15);
	rep->header.msgno 					= htonl(CLID_EXE_CMD_REPLY);
	rep->header.payloadLen 					= htonl(payload_length);

	rep->payload.clid_exe_cmd_reply.errorcode		= htonl(CLID_STATUS_OK);
	rep->payload.clid_exe_cmd_reply.payload_length		= htonl(total_len);
	memcpy(rep->payload.clid_exe_cmd_reply.payload, cmd_output, total_len);

	int res = send(sockfd, rep, msg_len, 0);
	if(res < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to send CLID_EXE_CMD_REPLY, errno = %d!", errno);
		return false;
	}

	free(rep);
	TPT_TRACE(TRACE_INFO, "Sent CLID_EXE_CMD_REPLY successfully!");
	return true;
}





