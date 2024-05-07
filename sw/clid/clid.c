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
#include <pthread.h>
#include <sys/timerfd.h>
#include <search.h>
#include <sys/ioctl.h>
#include <net/if.h>

// #include <itc.h>
#include "traceUtils.h"

/*****************************************************************************\/
*****                          INTERNAL TYPES                              *****
*******************************************************************************/
#define MAX_OF(a, b)		(a) > (b) ? (a) : (b)
#define MIN_OF(a, b)		(a) < (b) ? (a) : (b)
#define TCP_CLID_PORT		33333
#define MAX_NUM_SHELL_CLIENTS	255
#define NET_INTERFACE_ETH0	"eth0"
#define CLID_LOG_FILENAME	"clid.log"


struct clid_instance {
	int					tcp_fd;
	struct sockaddr_in			tcp_addr;
	// int					mbox_fd;
	// itc_mbox_id_t				mbox_id;
	// s
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
// static bool setup_mailbox(void);
static bool setup_tcp_server(void);
static struct in_addr get_ip_address_from_network_interface(int sockfd, char *interface);
static bool handle_accept_new_connection(int sockfd);



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

	if(!setup_tcp_server())
	{
		LOG_ERROR("Failed to setup clid daemon!");
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

		res = select(max_fd + 1, &fdset, NULL, NULL, NULL);
		if(res < 0)
		{
			LOG_ERROR("Failed to select()!");
			exit(EXIT_FAILURE);
		}

		if(FD_ISSET(clid_inst.tcp_fd, &fdset))
		{
			if(handle_accept_new_connection(clid_inst.tcp_fd) == false)
			{
				LOG_ERROR("Failed to handle_accept_new_connection()!");
				exit(EXIT_FAILURE);
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
	LOG_INFO("CLID is terminated with SIG = %d, calling exit handler...", signo);
	clid_exit_handler();

	// After clean up, resume raising the suppressed signal
	signal(signo, SIG_DFL); // Inform kernel does fault exit_handler for this kind of signal
	raise(signo);
}

static void clid_exit_handler(void)
{
	LOG_INFO("CLID is terminated, calling exit handler...");

	close(clid_inst.tcp_fd);
	
	LOG_INFO("CLID exit handler finished!");
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

// static bool setup_mailbox(void)
// {
// 	clid_inst.tcp_server_mbox_id = itc_create_mailbox(ITC_GATEWAY_MBOX_TCP_SER_NAME2, ITC_NO_NAMESPACE); // TEST ONLY
// 	if(clid_inst.tcp_server_mbox_id == ITC_NO_MBOX_ID)
// 	{
// 		LOG_ERROR("Failed to create mailbox %s", ITC_GATEWAY_MBOX_TCP_SER_NAME2); // TEST ONLY
// 		return false;
// 	}

// 	clid_inst.tcp_server_mbox_fd = itc_get_fd(clid_inst.tcp_server_mbox_id);
// 	LOG_INFO("Create TCP server mailbox \"%s\" successfully!", ITC_GATEWAY_MBOX_TCP_SER_NAME2); // TEST ONLY
// 	return true;
// }

static bool setup_tcp_server(void)
{
	int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
	if(tcpfd < 0)
	{
		LOG_ERROR("Failed to get socket(), errno = %d!", errno);
		return false;
	}

	int listening_opt = 1;
	int res = setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &listening_opt, sizeof(int));
	if(res < 0)
	{
		LOG_ERROR("Failed to set sockopt SO_REUSEADDR, errno = %d!", errno);
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
		LOG_ERROR("Failed to bind, errno = %d!", errno);
		close(tcpfd);
		return false;
	}

	res = listen(tcpfd, MAX_NUM_SHELL_CLIENTS);
	if(res < 0)
	{
		LOG_ERROR("Failed to listen, errno = %d!", errno);
		close(tcpfd);
		return false;
	}

	clid_inst.tcp_fd = tcpfd;

	LOG_INFO("Setup TCP server successfully on %s:%d", inet_ntoa(clid_inst.tcp_addr.sin_addr), ntohs(clid_inst.tcp_addr.sin_port));
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
		LOG_ERROR("Failed to ioctl to obtain IP address from %s, errno = %d!", interface, errno);
		return sock_addr.sin_addr;
	}

	size = sizeof(struct sockaddr_in);
	memcpy(&sock_addr, &(ifrq.ifr_ifru.ifru_addr), size);

	LOG_INFO("Retrieve address from network interface \"%s\" -> tcp://%s:%d", interface, inet_ntoa(sock_addr.sin_addr), sock_addr.sin_port);
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
			LOG_ABN("Accepting connection was interrupted, just ignore it!");
			return true;
		} else
		{
			LOG_ERROR("Accepting connection was destroyed!");
			return false;
		}
	}

	LOG_INFO("Receiving new connection from a peer client tcp://%s:%hu/", inet_ntoa(new_addr.sin_addr), ntohs(new_addr.sin_port));
	return true;
}





