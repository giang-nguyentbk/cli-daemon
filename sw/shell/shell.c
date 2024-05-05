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


/*
*  There are 3 types of commands:
*  	1. Local commands: are commands that we can run whether connecting to remote device or not.
*  	2. Remote built-in commands: are commands that we can run after connecting to remote device to get device's information.
*  	3. Remote registered commands: are commands that we can run after connecting to remote device to execute commands that is registered by remote device's application.
*/


/*****************************************************************************\/
*****                           INTERNAL TYPES                             *****
*******************************************************************************/
struct local_cmd {
	char	cmd[32];
	bool	(*handler)(char **args);
	char	description[128];
	char	syntax[64];
};

struct remote_host_info {
	char		hostname[32];
	char		ip[25];
	int		alive_timer_fd;
};

#define NUM_INTERNAL_CMDS	5
#define UDP_BROADCAST_PORT	11111
#define TCP_CLID_PORT		33333
#define MAX_NUM_REMOTE_HOSTS	255
#define CHECK_ALIVE_INTERVAL	15
#define MUTEX_LOCK(lock)								\
	do										\
	{										\
		pthread_mutex_lock(lock);						\
	} while(0)

#define MUTEX_UNLOCK(lock)								\
	do										\
	{										\
		pthread_mutex_unlock(lock);						\
	} while(0)

/*****************************************************************************\/
*****                         INTERNAL VARIABLES                           *****
*******************************************************************************/
static volatile bool m_is_sigint = false;
static bool m_is_exit = false;
static bool m_is_connected = false;
static char m_remote_ip[25] = "0.0.0.0";
static char m_buffer[256];
static int m_buff_len = 0;
static char *m_args[32];
static int m_nr_args = 0;
static struct local_cmd m_local_cmds[NUM_INTERNAL_CMDS];
static int m_udp_fd;
static struct remote_host_info m_remote_hosts[MAX_NUM_REMOTE_HOSTS];
static pthread_mutex_t m_remote_hosts_mtx;
static pthread_t m_udp_thread_id;
static pthread_mutex_t m_udp_thread_mtx;
static void *m_remote_host_tree;



/*****************************************************************************\/
*****                   INTERNAL FUNCTIONS PROTOTYPES                      *****
*******************************************************************************/
static void shell_init(void);
static void shell_sig_handler(int signo);
static bool read_input_cmd(char *buff);
static int get_token(const char *str, char *token);
static int get_args(char *cmd, char *args[]);
static int is_local_cmd(char *str);
static void execute_local_cmd(int index, char **args);
static bool setup_local_cmds(void);
static bool setup_udp_server(void);
static bool setup_udp_thread(void);
static void* udp_loop(void *data);
static bool setup_udp_remote_hosts(void);
static bool setup_alive_timer(int *timer_fd);
static bool restart_alive_timer(int timer_fd);
static int compare_hostname_remotehost_tree(const void *pa, const void *pb);
static int compare_host_remotehost_tree(const void *pa, const void *pb);
static bool handle_receive_broadcast_msg(int sockfd);

static bool local_help(char **args);
static bool local_exit(char **args);
static bool local_scan(char **args);
static bool local_connect(char **args);
static bool local_disconnect(char **args);


int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	// int opt = 0;
	// char tcp_ip[25];
	// int port = 0;

	shell_init();

	if(!setup_local_cmds())
	{
		printf("Failed to set local shell commands!\n");
		exit(EXIT_FAILURE);
	}

	// while((opt = getopt(argc, argv, "ip:")) != -1)
	// {
	// 	switch (opt)
	// 	{
	// 	case 'i':
	// 		strcpy(tcp_ip, optarg);
	// 		break;
		
	// 	case 'p':
	// 		port = atoi(optarg);
	// 		break;
	// 	default:
	// 		printf("ERROR: Usage:\t%s\t[-i ip_address]\t[-p port]\n", argv[0]);
	// 		printf("Example:\t%s\t-i   \"192.168.1.2\"\t-p   33333\n", argv[0]);
	// 		exit(EXIT_FAILURE);
	// 		break;
	// 	}
	// }

	// printf ("Connecting to device...\n");

	// int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	// if(sockfd < 0)
	// {
	// 	printf("Failed to get socket(), errno = %d!", errno);
	// 	return false;
	// }

	// struct sockaddr_in serveraddr;
	// memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	// serveraddr.sin_family = AF_INET;
	// serveraddr.sin_addr.s_addr = inet_addr(tcp_ip);
	// serveraddr.sin_port = htons(port);

	// int res = connect(sockfd, (struct sockaddr *)((void *)&serveraddr), sizeof(struct sockaddr_in));
	// if(res < 0)
	// {
	// 	printf("Failed to get connect(), errno = %d!", errno);
	// 	close(sockfd);
	// 	return false;
	// }

	// printf ("Connected to device: tcp://%s:%d\n", tcp_ip, port);

	setup_udp_server();
	setup_udp_thread();

	printf ("Starting new shell...\n");
	printf ("\n");

	int index = 0;
	while(!m_is_exit)
	{
		if(m_is_connected)
		{
			printf ("%s:%hu$ ", m_remote_ip, TCP_CLID_PORT);
		} else
		{
			printf ("local$ ");
		}

		m_buff_len = 0;
		bool ret = read_input_cmd(m_buffer);
		if(!ret)
		{
			exit(EXIT_FAILURE);
		}
		
		m_nr_args = 0;
		if(m_buff_len > 0)
		{
			m_nr_args = get_args(m_buffer, m_args);
		}

		if(m_nr_args >= 1)
		{
			if((index = is_local_cmd(m_args[0])) >= 0)
			{
				execute_local_cmd(index, m_args);
			} else
			{
				printf("%s: calling remote command!\n", m_args[0]);
			}
		}

		for(int i = 0; i < m_nr_args; i++)
		{
			free(m_args[i]);
			m_args[i] = NULL;
		}
	}

	exit(EXIT_SUCCESS);
}



/*****************************************************************************\/
*****                  INTERNAL FUNCTIONS IMPLEMENTATION                   *****
*******************************************************************************/
static void shell_init(void)
{
	// Customize Ctrl-C -> give up and terminate the on-going command -> Return to our shell ready for next commands
	struct sigaction sa;
	sa.sa_handler = &shell_sig_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
}

static void shell_sig_handler(int signo)
{
	(void)signo;
	m_is_sigint = true;

	// printf("\nCalling shell_sig_handler...\n");
}

static bool read_input_cmd(char *buff)
{
	if (fgets(buff, 256, stdin) != NULL)
	{
		m_buff_len = strlen(buff);
		if (m_buff_len > 0 && buff[m_buff_len-1] == '\n') {
			buff[--m_buff_len] = '\0';
		}
		// printf("You entered %d characters: \"%s\"\n", m_buff_len, buff);
	}
	else
	{
		if(!m_is_sigint)
		{
			printf("\nError reading from stdin!\n");
			return false;
		} else
		{
			// printf("\nReceived Ctrl C, just return to the shell!\n");
			m_is_sigint = false;
			m_buff_len = 0;
			printf ("\n");
		}
	}

	return true;
}

static int get_token(const char *str, char *token)
{
	int nr_chars = 0;
	// printf("get_token - Input str: \"%s\"\n", str);
	// If str has not reached NULL-terminated character '\0' yet and skip white spaces or tabs
	while(*str && strchr(" \t", *str))
	{
		str++;
		nr_chars++;
	}

	// Found new token
	bool is_negative = false;
	int i = 0;
	while(*str && !strchr(" \t", *str))
	{
		if(*str == '-')
		{
			if(i == 0)
			{
				is_negative = true;
				token[i] = *str++;
				i++;
				nr_chars++;
			} else
			{
				printf("Invalid argurment, found '-' in the middle of argument!\n");
				free(token);
				return 0;
			}
		}

		if((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'z') || (*str >= 'A' && *str <= 'Z'))
		{
			if(is_negative && (*str < '0' || *str > '9'))
			{
				printf("Invalid argurment, negative numeric argument but found alphabet character!\n");
				free(token);
				return 0;
			}

			token[i] = *str++;
			i++;
			nr_chars++;
		} else
		{
			printf("Invalid argurment, unknown character: '%c'\n", *str);
			free(token);
			return 0;
		}
	}

	token[i] = '\0';
	// printf("get_token - Token with %d characters: \"%s\"\n", i, token);

	if(strlen(token) <= 0)
	{
		free(token);
		return 0;
	}

	return nr_chars;
}

static int get_args(char *cmd, char *args[])
{
	char *token = malloc(32);
	
	int i = 0;
	int nr_chars = 0;
	while((nr_chars = get_token(cmd, token)) > 0)
	{
		args[i] = token;
		// printf("Token %d: \"%s\"\n", i, token);
		token = malloc(32);
		cmd += nr_chars;
		i++;
	}

	return i;
}

static int is_local_cmd(char *str)
{
	int i = 0;
	while(i < NUM_INTERNAL_CMDS)
	{
		if(strcmp(str,m_local_cmds[i].cmd) == 0)
		{
			return i;
		}
		
		i++;
	}

	return -1;
}

void execute_local_cmd(int index, char **args)
{
	if(m_local_cmds[index].handler == NULL)
	{
		printf("Handler for command %s not found!\n", m_local_cmds[index].cmd);
		return;
	}

	m_local_cmds[index].handler(args);
}

static bool setup_local_cmds(void)
{
	strcpy(m_local_cmds[0].cmd, "help");
	m_local_cmds[0].handler = &local_help;
	strcpy(m_local_cmds[0].description, "List all local and remote commands, respective syntaxes and description.");
	strcpy(m_local_cmds[0].syntax, "help");

	strcpy(m_local_cmds[1].cmd, "exit");
	m_local_cmds[1].handler = &local_exit;
	strcpy(m_local_cmds[1].description, "Disconnect from remote device and exit current shell.");
	strcpy(m_local_cmds[1].syntax, "exit");

	strcpy(m_local_cmds[2].cmd, "scan");
	m_local_cmds[2].handler = &local_scan;
	strcpy(m_local_cmds[2].description, "Scan remote devices being active.");
	strcpy(m_local_cmds[2].syntax, "scan");
	
	strcpy(m_local_cmds[3].cmd, "connect");
	m_local_cmds[3].handler = &local_connect;
	strcpy(m_local_cmds[3].description, "Connect to remote device via ip address and port or index returned by scan.");
	strcpy(m_local_cmds[3].syntax, "connect { <ip> <port> | <index> }");
	
	strcpy(m_local_cmds[4].cmd, "disconnect");
	m_local_cmds[4].handler = &local_disconnect;
	strcpy(m_local_cmds[4].description, "Disconnect from an already connected remote device.");
	strcpy(m_local_cmds[4].syntax, "disconnect");

	return true;
}

static bool local_help(char **args)
{
	(void)args;

	if(m_nr_args > 1)
	{
		printf("help: Too many arguments!\n\n");
		return false;
	}

	printf("%-64s %-128s\n", "Syntax", "Description");
	printf("%-64s %-128s\n", "------", "-----------");
	for(int i = 0; i < NUM_INTERNAL_CMDS; i++)
	{
		printf("%-64s %-128s\n", m_local_cmds[i].syntax, m_local_cmds[i].description);
	}
	printf("\n");

	printf("Syntax Explanation:\n");
	printf("%-20s %-128s\n", "Syntax Form", "Meaning");
	printf("%-20s %-128s\n", "-----------", "-------");
	printf("%-20s %-128s\n", "abc", "string literal value");
	printf("%-20s %-128s\n", "<abc>", "any value");
	printf("%-20s %-128s\n", "{ abc | def }", "alternative values");
	printf("%-20s %-128s\n", "[ abc ]", "optional value");
	printf("%-20s %-128s\n", "[ abc | def ]", "optional alternative values, simplified form of [ { abc | def } ]");
	printf("\n");

	return true;
}

static bool local_exit(char **args)
{
	(void)args;
	printf("exit: calling this command!\n");

	printf("\n");
	m_is_exit = true;
	return true;
}

static bool local_scan(char **args)
{
	(void)args;

	if(m_nr_args > 1)
	{
		printf("scan: Too many arguments!\n\n");
		return false;
	}

	printf("%-10s %-32s %-25s %-7s\n", "Unique ID", "Hostname", "IP Address", "Port");
	printf("%-10s %-32s %-25s %-7s\n", "---------", "--------", "----------", "----");
	MUTEX_LOCK(&m_remote_hosts_mtx);
	for(int i = 0; i < MAX_NUM_REMOTE_HOSTS; i++)
	{
		if(strcmp(m_remote_hosts[i].hostname, "") != 0)
		{
			char index[5];
			char port[7];
			sprintf(index, "%d", i);
			sprintf(port, "%hu", TCP_CLID_PORT);
			printf("%-10s %-32s %-25s %-7s\n", index, m_remote_hosts[i].hostname, m_remote_hosts[i].ip, port);
		}
	}
	MUTEX_UNLOCK(&m_remote_hosts_mtx);
	printf("\n");

	return true;
}

static bool local_connect(char **args)
{
	(void)args;
	printf("connect: calling this command!\n");
	printf("\n");
	m_is_connected = true;
	return true;
}

static bool local_disconnect(char **args)
{
	(void)args;
	printf("disconnect: calling this command!\n");
	printf("\n");
	m_is_connected = false;
	return true;
}

static bool setup_udp_server(void)
{
	m_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(m_udp_fd < 0)
	{
		printf("Failed to create UDP socket(), errno = %d!\n", errno);
		return false;
	}

	int broadcast_opt = 1;
	int res = setsockopt(m_udp_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_opt, sizeof(int));
	if(res < 0)
	{
		printf("Failed to setsockopt() SO_BROADCAST, errno = %d!\n", errno);
		close(m_udp_fd);
		return false;
	}

	res = setsockopt(m_udp_fd, SOL_SOCKET, SO_REUSEADDR, &broadcast_opt, sizeof(int));
	if(res < 0)
	{
		printf("Failed to setsockopt() SO_REUSEADDR, errno = %d!\n", errno);
		close(m_udp_fd);
		return false;
	}

	struct sockaddr_in myUDPaddr;
	size_t size = sizeof(struct sockaddr_in);
	memset(&myUDPaddr, 0, size);
	myUDPaddr.sin_family = AF_INET;
	myUDPaddr.sin_addr.s_addr = INADDR_ANY;
	myUDPaddr.sin_port = htons(UDP_BROADCAST_PORT);

	res = bind(m_udp_fd, (struct sockaddr *)((void *)&myUDPaddr), size);
	if(res < 0)
	{
		printf("Failed to bind(), errno = %d!\n", errno);
		close(m_udp_fd);
		return false;
	}

	// printf("Setup my UDP successfully on %s:%d\n", inet_ntoa(myUDPaddr.sin_addr), ntohs(myUDPaddr.sin_port));
	return true;
}

static bool setup_udp_thread(void)
{
	int res = pthread_mutex_init(&m_remote_hosts_mtx, NULL);
	if(res != 0)
	{
		printf("Failed to pthread_mutex_init server, error code = %d\n", res);
		return false;
	}
	
	res = pthread_mutex_init(&m_udp_thread_mtx, NULL);
	if(res != 0)
	{
		printf("Failed to pthread_mutex_init server, error code = %d\n", res);
		return false;
	}

	MUTEX_LOCK(&m_udp_thread_mtx);
	res = pthread_create(&m_udp_thread_id, NULL, udp_loop, NULL);
	if(res != 0)
	{
		printf("Failed to pthread_create, error code = %d\n", res);
		return false;
	}
	MUTEX_LOCK(&m_udp_thread_mtx);
	MUTEX_UNLOCK(&m_udp_thread_mtx);

	return true;
}

static void* udp_loop(void *data)
{
	(void)data;

	setup_udp_remote_hosts();

	MUTEX_UNLOCK(&m_udp_thread_mtx);
	fd_set fdset;
	int max_fd = -1;
	int res = 0;
	while(!m_is_exit)
	{
		FD_ZERO(&fdset);
		FD_SET(m_udp_fd, &fdset);
		max_fd = m_udp_fd + 1;

		MUTEX_LOCK(&m_remote_hosts_mtx);
		for(int i = 0; i < MAX_NUM_REMOTE_HOSTS; i++)
		{
			if(m_remote_hosts[i].alive_timer_fd != -1)
			{
				FD_SET(m_remote_hosts[i].alive_timer_fd, &fdset);
				max_fd = m_remote_hosts[i].alive_timer_fd >= max_fd ? m_remote_hosts[i].alive_timer_fd + 1 : max_fd;
			}
		}
		MUTEX_UNLOCK(&m_remote_hosts_mtx);


		res = select(max_fd, &fdset, NULL, NULL, NULL);
		if(res < 0)
		{
			printf("Failed to select() in TCP server loop!\n");
			return NULL;
		}

		if(FD_ISSET(m_udp_fd, &fdset))
		{
			if(handle_receive_broadcast_msg(m_udp_fd) == false)
			{
				printf("Failed to handle_receive_broadcast_msg()!\n");
				return NULL;
			}
		}

		MUTEX_LOCK(&m_remote_hosts_mtx);
		for(int i = 0; i < MAX_NUM_REMOTE_HOSTS; i++)
		{
			if((FD_ISSET(m_remote_hosts[i].alive_timer_fd, &fdset) && m_remote_hosts[i].alive_timer_fd != -1))
			{
				tdelete(m_remote_hosts[i].hostname, &m_remote_host_tree, compare_hostname_remotehost_tree);
				strcpy(m_remote_hosts[i].hostname, "");
				strcpy(m_remote_hosts[i].ip, "0.0.0.0");
				close(m_remote_hosts[i].alive_timer_fd);
				m_remote_hosts[i].alive_timer_fd = -1;
			}
		}
		MUTEX_UNLOCK(&m_remote_hosts_mtx);
	}

	close(m_udp_fd);
	return NULL;
}

static bool handle_receive_broadcast_msg(int sockfd)
{
	struct sockaddr_in m_peerUDPaddr;
	int MAX_ETHERNET_PACKET_SIZE = 1500;
	char rx_buff[MAX_ETHERNET_PACKET_SIZE];
	socklen_t length = sizeof(struct sockaddr_in);

	memset(&m_peerUDPaddr, 0, length);

	int res = recvfrom(sockfd, rx_buff, MAX_ETHERNET_PACKET_SIZE, 0, (struct sockaddr *)((void *)&m_peerUDPaddr), &length);
	if(res < 0)
	{
		if(errno != EINTR)
		{
			printf("Failed to recvfrom(), errno = %d!\n", errno);
		} else
		{
			printf("Receiving message was interrupted, continue receiving!\n");
		}
		return false;
	}

	rx_buff[res] = '\0';

	char hostname[32];
	char tcp_ip[25];
	uint16_t tcp_port;
	/* Instead of format string "%s" as usual, we must use "%[^:]" meaning read to string tcp_ip until character ':'. */
	res = sscanf(rx_buff, "Broadcast Message: ITCGW from host <%[^>]> listening on tcp://%[^:]:%hu/", hostname, tcp_ip, &tcp_port);
	// printf("Received a greeting message from hostname <%s> on tcp://%s:%hu/\n", hostname, tcp_ip, tcp_port);

	MUTEX_LOCK(&m_remote_hosts_mtx);
	struct remote_host_info **iter;
	iter = tfind(hostname, &m_remote_host_tree, compare_hostname_remotehost_tree);
	if(iter != NULL)
	{
		/* Already added in tree */
		// printf("Already connected, ignore broadcasting message from this remote device!\n");
		restart_alive_timer((*iter)->alive_timer_fd);
		MUTEX_UNLOCK(&m_remote_hosts_mtx);
		return true;
	} else
	{
		int i = 0;
		for(; i < MAX_NUM_REMOTE_HOSTS; i++)
		{
			if(strcmp(m_remote_hosts[i].hostname, "") == 0)
			{
				strcpy(m_remote_hosts[i].hostname, hostname);
				strcpy(m_remote_hosts[i].ip, tcp_ip);
				setup_alive_timer(&(m_remote_hosts[i].alive_timer_fd));
				restart_alive_timer(m_remote_hosts[i].alive_timer_fd);
				tsearch(&m_remote_hosts[i], &m_remote_host_tree, compare_host_remotehost_tree);
				break;
			}
		}

		if(i == MAX_NUM_REMOTE_HOSTS)
		{
			printf("No more than %d devices is accepted!\n", MAX_NUM_REMOTE_HOSTS);
			return false;
		}
	}
	MUTEX_UNLOCK(&m_remote_hosts_mtx);

	return true;
}

static bool setup_alive_timer(int *timer_fd)
{
	*timer_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if(*timer_fd < 0)
	{
		printf("Failed to timerfd_create(), errno = %d!\n", errno);
		return false;
	}

	// printf("Alive timer fd %d created successfully!\n", *stimer_fd);
	return true;
}

static bool restart_alive_timer(int timer_fd)
{
	if(timer_fd < 0)
	{
		printf("Failed to restart_alive_timer due to timer_fd = %d, errno = %d!\n", timer_fd, errno);
		return false;
	}

	struct timespec now;
	struct itimerspec its;

	clock_gettime(CLOCK_REALTIME, &now);
	memset(&its, 0, sizeof(struct itimerspec));
	its.it_value.tv_sec = now.tv_sec + (time_t)CHECK_ALIVE_INTERVAL;
	its.it_value.tv_nsec = now.tv_nsec;
	int res = timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL);
	if(res < 0)
	{
		printf("Failed to timerfd_settime(), errno = %d!\n", errno);
		return false;
	}

	return true;
}

static bool setup_udp_remote_hosts(void)
{
	MUTEX_LOCK(&m_remote_hosts_mtx);
	for(int i = 0; i < MAX_NUM_REMOTE_HOSTS; i++)
	{
		strcpy(m_remote_hosts[i].hostname, "");
		strcpy(m_remote_hosts[i].ip, "0.0.0.0");
		m_remote_hosts[i].alive_timer_fd = -1;
	}
	MUTEX_UNLOCK(&m_remote_hosts_mtx);

	return true;
}

static int compare_hostname_remotehost_tree(const void *pa, const void *pb)
{
	const char *hostname = pa;
	const struct remote_host_info *peer = pb;

	return strcmp(hostname, peer->hostname);
}

static int compare_host_remotehost_tree(const void *pa, const void *pb)
{
	const struct remote_host_info *peer_a = pa;
	const struct remote_host_info *peer_b = pb;
	
	return strcmp(peer_a->hostname, peer_b->hostname);
}








