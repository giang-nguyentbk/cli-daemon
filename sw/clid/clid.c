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

#include "cli-daemon-tpt-provider.h"
#include "tcp_proto.h"
#include "cmdProto.h"

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
#define NET_INTERFACE_ETH0	"eth0"
#define CLID_LOG_FILENAME	"clid.log"
#define CLID_MBOX_NAME		"clidMailbox"


struct shell_client {
	int			fd;
	int			job_timer_fd;
	unsigned long long	current_job_id;
};

struct command {
	itc_mbox_id_t		mbox_id;
	char			cmd_name[MAX_CMD_NAME_LENGTH];
	char			cmd_desc[MAX_CMD_DESC_LENGTH];
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
static unsigned long long m_job_id = 0; // global job id, each requested cmd execution from a shell client has a unique job_id (count up to max of unsigned long long)


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
static int compare_timerfd_in_client_tree(const void *pa, const void *pb);
static int compare_jobid_in_client_tree(const void *pa, const void *pb);
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
static bool send_exe_cmd_reply(int sockfd, uint32_t result, char *output);
static bool restart_job_timer(int sockfd, time_t timeout);
static bool set_time_job_timer(int timerfd, time_t timeout);
static bool is_job_timer_running(int timerfd);
static bool reassign_current_job_id(int sockfd, unsigned long long new_job_id);
static bool handle_receive_itc_msg(int mbox_fd);
static bool handle_receive_reg_cmd_request(union itc_msg *msg);
static int compare_cmdname_in_cmd_tree(const void *pa, const void *pb);
static int compare_command_in_cmd_tree(const void *pa, const void *pb);
static bool handle_receive_dereg_cmd_request(union itc_msg *msg);
static bool forward_exe_cmd_request(unsigned long long job_id, char *cmd_name, uint16_t num_args, uint32_t pl_len, char *pl);
static bool handle_receive_exe_cmd_reply(union itc_msg *msg);
static bool handle_job_timer_expired(int timerfd);




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
		FD_SET(clid_inst.mbox_fd, &fdset);
		max_fd = MAX_OF(clid_inst.mbox_fd, max_fd);

		for(int i = 0; i < MAX_NUM_SHELL_CLIENTS; i++)
		{
			if(clid_inst.clients[i].fd != -1)
			{
				FD_SET(clid_inst.clients[i].fd, &fdset);
				max_fd = MAX_OF(clid_inst.clients[i].fd, max_fd);

				if(clid_inst.clients[i].job_timer_fd != -1 && is_job_timer_running(clid_inst.clients[i].job_timer_fd))
				{
					FD_SET(clid_inst.clients[i].job_timer_fd, &fdset);
					max_fd = MAX_OF(clid_inst.clients[i].job_timer_fd, max_fd);
				}
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

		if(FD_ISSET(clid_inst.mbox_fd, &fdset))
		{
			if(handle_receive_itc_msg(clid_inst.mbox_fd) == false)
			{
				TPT_TRACE(TRACE_ERROR, "Failed to handle_receive_itc_msg()!");
				exit(EXIT_FAILURE);
			}
		}

		for(int i = 0; i < MAX_NUM_SHELL_CLIENTS; i++)
		{
			if(clid_inst.clients[i].fd != -1)
			{
				if(clid_inst.clients[i].job_timer_fd != -1 && FD_ISSET(clid_inst.clients[i].job_timer_fd, &fdset))
				{
					if(handle_job_timer_expired(clid_inst.clients[i].job_timer_fd) == false)
					{
						TPT_TRACE(TRACE_ERROR, "Failed to handle_job_timer_expired()!");
						exit(EXIT_FAILURE);
					}
				}

				if(FD_ISSET(clid_inst.clients[i].fd, &fdset))
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
	tdestroy(clid_inst.cmd_tree, do_nothing);
	itc_delete_mailbox(clid_inst.mbox_id);
	itc_exit();
	
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

	clid_inst.mbox_fd = itc_get_fd();
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

static int compare_timerfd_in_client_tree(const void *pa, const void *pb)
{
	const int *timerfd = pa;
	const struct shell_client *client = pb;

	if(*timerfd == client->job_timer_fd)
	{
		return 0;
	} else if(*timerfd > client->job_timer_fd)
	{
		return 1;
	} else
	{
		return -1;
	}
}

static int compare_jobid_in_client_tree(const void *pa, const void *pb)
{
	const unsigned long long *job_id = pa;
	const struct shell_client *client = pb;

	if(*job_id == client->current_job_id)
	{
		return 0;
	} else if(*job_id > client->current_job_id)
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

static int compare_cmdname_in_cmd_tree(const void *pa, const void *pb)
{
	const char *cmdname_a = pa;
	const struct command *command_b = pb;
	
	return strcmp(cmdname_a, command_b->cmd_name);
}

static int compare_command_in_cmd_tree(const void *pa, const void *pb)
{
	const struct command *command_a = pa;
	const struct command *command_b = pb;
	
	return strcmp(command_a->cmd_name, command_b->cmd_name);
}

static bool setup_shell_clients(void)
{
	for(int i = 0; i < MAX_NUM_SHELL_CLIENTS; i++)
	{
		clid_inst.clients[i].fd = -1;
		clid_inst.clients[i].job_timer_fd = -1;
		clid_inst.clients[i].current_job_id = 0;
	}

	return true;
}

static bool setup_command_list(void)
{
	clid_inst.cmd_count = 0;

	// strcpy(clid_inst.cmds[0].cmd_name, "aclocal");
	// strcpy(clid_inst.cmds[0].cmd_desc, "Run aclocal command.");
	// clid_inst.cmd_count++;

	// strcpy(clid_inst.cmds[1].cmd_name, "acreconf");
	// strcpy(clid_inst.cmds[1].cmd_desc, "Run acreconf command.");
	// clid_inst.cmd_count++;

	// strcpy(clid_inst.cmds[2].cmd_name, "aclog");
	// strcpy(clid_inst.cmds[2].cmd_desc, "Run aclog command.");
	// clid_inst.cmd_count++;

	for(int i = 0; i < MAX_NUM_CMDS; i++)
	{
		clid_inst.cmds[i].mbox_id = ITC_NO_MBOX_ID;
		clid_inst.cmds[i].cmd_name[0] = '\0';
		clid_inst.cmds[i].cmd_desc[0] = '\0';
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
		TPT_TRACE(TRACE_INFO, "Received CLID_EXE_CMD_REQUEST!");
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
	struct shell_client **iter;
	iter = tfind(&sockfd, &clid_inst.client_tree, compare_fd_in_client_tree);
	if(iter == NULL)
	{
		TPT_TRACE(TRACE_ABN, "Disconnected shell client not found in the tree, something wrong!");
		return false;
	}

	close(sockfd);
	(*iter)->fd = -1;

	if((*iter)->job_timer_fd != -1 && !set_time_job_timer((*iter)->job_timer_fd, 0))
	{
		TPT_TRACE(TRACE_ERROR, "Could not stop job timer fd = %d!", (*iter)->job_timer_fd);
		return false;
	}

	close((*iter)->job_timer_fd);
	(*iter)->job_timer_fd = -1;

	(*iter)->current_job_id = 0;

	tdelete(*iter, &clid_inst.client_tree, compare_client_in_client_tree);

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
	char cmd_name[MAX_CMD_NAME_LENGTH];

	strcpy(cmd_name, (req->payload + offset));
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: cmd_name_len: %hu", strlen(cmd_name));
	offset += strlen(cmd_name) + 1;
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: cmd_name: %s", cmd_name);

	uint16_t num_args = *((uint16_t *)(req->payload + offset));
	offset += 2;
	TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: num_args: %hu", num_args);

	char *payload = (req->payload + offset);
	uint32_t payload_len = req->payload_length - offset;

	/* DEBUG PURPOSE ONLY */
	char args[MAX_CMD_NAME_LENGTH];
	for(int i = 0; i < num_args; i++)
	{
		/* This is arguments */
		strcpy(args, (req->payload + offset));
		offset += strlen(args);
		TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: arg_len %d: %hu", i, strlen(args));
		TPT_TRACE(TRACE_INFO, "Re-interpret TCP packet: args %d: %s", i, args);
	}

	// Prepare a new job_id assigned to this execution
	// Prevent from the case unsigned long long is overflowed, jump from maxof(unsigned long long) to 1 directly.
	// Value of 0 indicates current client has no job execution running, in idle state.
	unsigned long long new_job_id = ++m_job_id ? m_job_id : ++m_job_id;

	// Restart job_timer for this shell client
	if(!restart_job_timer(sockfd, (time_t)req->timeout))
	{
		TPT_TRACE(TRACE_ERROR, "Failed to restart_job_timer() for this new execution, sockfd = %d", sockfd);
		return false;
	}

	if(!reassign_current_job_id(sockfd, new_job_id))
	{
		TPT_TRACE(TRACE_ERROR, "Failed to reassign_current_job_id() for this new execution, sockfd = %d, job_id = %llu", sockfd, new_job_id);
		return false;
	}

	if(!forward_exe_cmd_request(new_job_id, cmd_name, num_args, payload_len, payload))
	{
		return false;
	}

	TPT_TRACE(TRACE_INFO, "Forward new job execution sockfd = %d, job_id = %llu", sockfd, new_job_id);
	
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

static bool send_exe_cmd_reply(int sockfd, uint32_t result, char *output)
{
	/* MOCK PURPOSE ONLY */
	// char cmd_output[512];
	// strcpy(cmd_output, "This is a notification toward user notifying that the command was executing successfully!");
	// uint32_t output_len = strlen(cmd_output);

	uint32_t output_len = strlen(output);

	size_t msg_len = offsetof(struct ethtcp_msg, payload) + offsetof(struct clid_exe_cmd_reply, payload) + output_len + 1;
	struct ethtcp_msg *rep = malloc(msg_len);
	if(rep == NULL)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to malloc locate mbox reply message!");
		return false;
	}

	uint32_t payload_length = offsetof(struct clid_exe_cmd_reply, payload) + output_len + 1;
	rep->header.sender 					= htonl((uint32_t)getpid());
	rep->header.receiver 					= htonl(111);
	rep->header.protRev 					= htonl(15);
	rep->header.msgno 					= htonl(CLID_EXE_CMD_REPLY);
	rep->header.payloadLen 					= htonl(payload_length);

	rep->payload.clid_exe_cmd_reply.errorcode		= htonl(CLID_STATUS_OK);
	rep->payload.clid_exe_cmd_reply.result			= htonl(result);
	rep->payload.clid_exe_cmd_reply.payload_length		= htonl(output_len + 1);
	memcpy(rep->payload.clid_exe_cmd_reply.payload, output, output_len + 1);

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

static bool restart_job_timer(int sockfd, time_t timeout)
{
	struct shell_client **iter;
	iter = tfind(&sockfd, &clid_inst.client_tree, compare_fd_in_client_tree);
	if(iter == NULL)
	{
		TPT_TRACE(TRACE_ABN, "This fd %d not found in client tree, something wrong!", sockfd);
		return false;
	}

	if((*iter)->job_timer_fd <= 0)
	{
		// The first job for this shell client
		TPT_TRACE(TRACE_INFO, "Starting the first job for this shell client, sockfd = %d", sockfd);
		(*iter)->job_timer_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
		if((*iter)->job_timer_fd < 0)
		{
			TPT_TRACE(TRACE_ERROR, "Failed to timerfd_create(), errno = %d!", errno);
			return false;
		}

		;
	}

	// Whether either one of two scenarios below happens, do a same thing.
	// 1. There is another job for this client still running, skip it.
	// When job results of the discarded job sent back to clid daemon from application threads just discard it
	// because current_job_id for this shell client should be changed by the caller of this function,
	// The shell client did not need the discarded job anymore (probably they press Ctrl-C and send us this new job).
	// 2. No job is running for this shell client, set new timeout for the job_timer

	if(!set_time_job_timer((*iter)->job_timer_fd, timeout))
	{
		TPT_TRACE(TRACE_ERROR, "Could not start job timer for this shell client sockfd = %d!", sockfd);
		return false;
	}

	return true;
}

static bool set_time_job_timer(int timerfd, time_t timeout)
{
	struct timespec now;
	struct itimerspec its;
	int res = -1;

	clock_gettime(CLOCK_REALTIME, &now);
	memset(&its, 0, sizeof(struct itimerspec));
	its.it_value.tv_sec = now.tv_sec + (time_t)timeout;
	its.it_value.tv_nsec = now.tv_nsec;
	res = timerfd_settime(timerfd, TFD_TIMER_ABSTIME, &its, NULL);
	if(res < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to timerfd_settime(), errno = %d!", errno);
		return false;
	}

	return true;
}

static bool is_job_timer_running(int timerfd)
{
	struct itimerspec remaining_time;

	int res = timerfd_gettime(timerfd, &remaining_time);
	if(res < 0)
	{
		TPT_TRACE(TRACE_ERROR, "Failed to timerfd_gettime(), errno = %d!", errno);
		return false;
	}

	TPT_TRACE(TRACE_INFO, "Job timer for timerfd %d will expire in: %ld.%ld seconds!", timerfd, remaining_time.it_value.tv_sec, remaining_time.it_value.tv_nsec / 1000000);

	if(remaining_time.it_value.tv_sec == 0 && remaining_time.it_value.tv_nsec == 0)
	{
		return false;
	}

	return true;
}

static bool reassign_current_job_id(int sockfd, unsigned long long new_job_id)
{
	struct shell_client **iter;
	iter = tfind(&sockfd, &clid_inst.client_tree, compare_fd_in_client_tree);
	if(iter == NULL)
	{
		TPT_TRACE(TRACE_ABN, "This fd %d not found in client tree, something wrong!", sockfd);
		return false;
	}

	(*iter)->current_job_id = new_job_id;

	return true;
}

static bool handle_receive_itc_msg(int mbox_fd)
{
	(void)mbox_fd;
	union itc_msg *msg;

	msg = itc_receive(ITC_NO_WAIT);

	if(msg == NULL)
	{
		TPT_TRACE(TRACE_ERROR, "Fatal error, clid daemon received a NULL itc_msg!");
		return false;
	}

	switch (msg->msgno)
	{
	case CMDIF_REG_CMD_REQUEST:
		TPT_TRACE(TRACE_INFO, "Received CMDIF_REG_CMD_REQUEST mbox_id = 0x%08x", msg->cmdIfRegCmdRequest.mbox_id);
		TPT_TRACE(TRACE_INFO, "Received CMDIF_REG_CMD_REQUEST cmd_name = %s", msg->cmdIfRegCmdRequest.cmd_name);
		TPT_TRACE(TRACE_INFO, "Received CMDIF_REG_CMD_REQUEST cmd_desc = %s", msg->cmdIfRegCmdRequest.cmd_desc);
		handle_receive_reg_cmd_request(msg);
		break;

	case CMDIF_DEREG_CMD_REQUEST:
		TPT_TRACE(TRACE_INFO, "Received CMDIF_DEREG_CMD_REQUEST cmd_name = %s", msg->cmdIfDeregCmdRequest.cmd_name);
		handle_receive_dereg_cmd_request(msg);
		break;

	case CMDIF_EXE_CMD_REPLY:
		TPT_TRACE(TRACE_INFO, "Received CMDIF_EXE_CMD_REPLY output = %s", msg->cmdIfExeCmdReply.output);
		handle_receive_exe_cmd_reply(msg);
		break;

	default:
		TPT_TRACE(TRACE_ABN, "Received invalid message msgno = 0x%08x", msg->msgno);
		break;
	}

	itc_free(&msg);
	return true;
}

static bool handle_receive_reg_cmd_request(union itc_msg *msg)
{
	struct command **iter;
	iter = tfind(msg->cmdIfRegCmdRequest.cmd_name, &clid_inst.cmd_tree, compare_cmdname_in_cmd_tree);
	if(iter != NULL)
	{
		TPT_TRACE(TRACE_ABN, "This cmdName %s already registered by mailbox id 0x%08x, something abnormal!", msg->cmdIfRegCmdRequest.cmd_name, (*iter)->mbox_id);
		return true;
	}

	int i = 0;
	for(; i < MAX_NUM_CMDS; i++)
	{
		if(clid_inst.cmds[i].cmd_name[0] == '\0')
		{
			clid_inst.cmds[i].mbox_id = msg->cmdIfRegCmdRequest.mbox_id;
			strcpy(clid_inst.cmds[i].cmd_name, msg->cmdIfRegCmdRequest.cmd_name);
			strcpy(clid_inst.cmds[i].cmd_desc, msg->cmdIfRegCmdRequest.cmd_desc);
			tsearch(&clid_inst.cmds[i], &clid_inst.cmd_tree, compare_command_in_cmd_tree);
			clid_inst.cmd_count++;
			break;
		}
	}

	if(i == MAX_NUM_CMDS)
	{
		TPT_TRACE(TRACE_ERROR, "No more than %d cmdName is accepted to be registered!", MAX_NUM_CMDS);
		return false;
	}

	return true;
}

static bool handle_receive_dereg_cmd_request(union itc_msg *msg)
{
	struct command **iter;
	iter = tfind(msg->cmdIfDeregCmdRequest.cmd_name, &clid_inst.cmd_tree, compare_cmdname_in_cmd_tree);
	if(iter == NULL)
	{
		TPT_TRACE(TRACE_ABN, "This cmdName %s not registered yet, something wrong!", msg->cmdIfDeregCmdRequest.cmd_name);
		return true;
	}

	(*iter)->mbox_id = ITC_NO_MBOX_ID;
	(*iter)->cmd_name[0] = '\0';
	(*iter)->cmd_desc[0] = '\0';

	tdelete(*iter, &clid_inst.cmd_tree, compare_command_in_cmd_tree);

	return true;
}

static bool forward_exe_cmd_request(unsigned long long job_id, char *cmd_name, uint16_t num_args, uint32_t pl_len, char *pl)
{
	struct command **iter;
	iter = tfind(cmd_name, &clid_inst.cmd_tree, compare_cmdname_in_cmd_tree);
	if(iter == NULL)
	{
		TPT_TRACE(TRACE_ABN, "This cmdName %s not found in command tree, something abnormal!", cmd_name);
		return true;
	}

	union itc_msg* fwd = itc_alloc(offsetof(struct CmdIfExeCmdRequestS, payload) + pl_len, CMDIF_EXE_CMD_REQUEST);

	// TPT_TRACE(TRACE_DEBUG, "fwd = 0x%016x", fwd);
	// TPT_TRACE(TRACE_DEBUG, "job_id = 0x%016x", &(fwd->cmdIfExeCmdRequest.job_id));
	// TPT_TRACE(TRACE_DEBUG, "cmd_name = 0x%016x", (unsigned long long)(fwd->cmdIfExeCmdRequest.cmd_name));
	// TPT_TRACE(TRACE_DEBUG, "num_args = 0x%016x", &(fwd->cmdIfExeCmdRequest.num_args));
	// TPT_TRACE(TRACE_DEBUG, "payloadLen = 0x%016x", &(fwd->cmdIfExeCmdRequest.payloadLen));
	// TPT_TRACE(TRACE_DEBUG, "payload = 0x%016x", (unsigned long long)(fwd->cmdIfExeCmdRequest.payload));

	fwd->cmdIfExeCmdRequest.job_id = job_id;
	memset(fwd->cmdIfExeCmdRequest.cmd_name, 0, MAX_CMD_NAME_LENGTH);
	strcpy(fwd->cmdIfExeCmdRequest.cmd_name, cmd_name);
	fwd->cmdIfExeCmdRequest.num_args = num_args;
	fwd->cmdIfExeCmdRequest.payloadLen = pl_len;
	memcpy(fwd->cmdIfExeCmdRequest.payload, pl, pl_len);

	if(!itc_send(&fwd, (*iter)->mbox_id, ITC_MY_MBOX_ID, NULL))
	{
		TPT_TRACE(TRACE_ERROR, "Failed to send CMDIF_EXE_CMD_REQUEST for cmdName %s to mbox id 0x%08x", cmd_name, (*iter)->mbox_id);
		return false;
	}

	TPT_TRACE(TRACE_INFO, "Forwarded CMDIF_EXE_CMD_REQUEST for cmdName %s to mbox id 0x%08x", cmd_name, (*iter)->mbox_id);
	return true;

}

static bool handle_receive_exe_cmd_reply(union itc_msg *msg)
{
	struct shell_client **iter;
	iter = tfind(&(msg->cmdIfExeCmdReply.job_id), &clid_inst.client_tree, compare_jobid_in_client_tree);
	if(iter == NULL)
	{
		// There are some potential situation:
		// 1. Job timer was already expired
		// 2. Shell client pressed Ctrl-C to cancel the previous job execution
		// 3. Shell client was disconnected
		// -> Suggest to check log's flow to see what is the reason

		TPT_TRACE(TRACE_ABN, "Received CMDIF_EXE_CMD_REPLY, job_id = %d, which is not valid anymore, something wrong!", msg->cmdIfExeCmdReply.job_id);
		return true;
	}

	if(!send_exe_cmd_reply((*iter)->fd, msg->cmdIfExeCmdReply.result, msg->cmdIfExeCmdReply.output))
	{
		return false;
	}

	// Done this job execution, stop the respective job timer and unset current_job_id.
	if((*iter)->job_timer_fd != -1 && !set_time_job_timer((*iter)->job_timer_fd, 0))
	{
		TPT_TRACE(TRACE_ERROR, "Could not stop job timer fd = %d!", (*iter)->job_timer_fd);
		return false;
	}

	(*iter)->current_job_id = 0;

	return true;
}

static bool handle_job_timer_expired(int timerfd)
{
	struct shell_client **iter;
	iter = tfind(&timerfd, &clid_inst.client_tree, compare_timerfd_in_client_tree);
	if(iter == NULL)
	{
		TPT_TRACE(TRACE_ABN, "Job timer fd = %d not found in client tree, something wrong!", timerfd);
		return true;
	}

	char *output = "Expired!";
	if(!send_exe_cmd_reply((*iter)->fd, (uint32_t)CMDIF_RET_FAIL, output))
	{
		return false;
	}

	(*iter)->current_job_id = 0;

	return true;
}





























