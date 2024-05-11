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
#include <stddef.h>

#include "tcp_proto.h"


/*
*  There are 3 types of commands:
*  	1. Local commands: are commands that we can run whether connecting to remote device or not.
*  	2. Remote built-in commands: are commands that we can run after connecting to remote device to get device's information.
*  	3. Remote registered commands: are commands that we can run after connecting to remote device to execute commands that is registered by remote device's application.
*/


/*****************************************************************************\/
*****                           INTERNAL TYPES                             *****
*******************************************************************************/
#define NUM_INTERNAL_CMDS	6
#define MAX_REMOTE_CMDS		255
#define MAX_HISTORY_CMDS	50
#define MAX_ARG_LENGTH		64
#define MAX_NUM_ARGS		32
#define MAX_HOST_NAME_LENGTH	255
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

struct local_cmd {
	char	cmd[MAX_ARG_LENGTH];
	bool	(*handler)(char **args);
	char	description[128];
	char	syntax[64];
};

struct remote_cmd {
	char	cmd[MAX_ARG_LENGTH];
	char	description[128];
};

struct remote_host_info {
	char		hostname[MAX_HOST_NAME_LENGTH];
	char		ip[25];
	int		alive_timer_fd;
};

struct history_cmd {
	struct history_cmd	*next;
	struct history_cmd	*prev;

	char			cmd[256];
};

struct history_cmd_queue {
	struct history_cmd	*head;
	struct history_cmd	*tail;
};


/*****************************************************************************\/
*****                         INTERNAL VARIABLES                           *****
*******************************************************************************/
static volatile bool m_is_sigint = false;
static bool m_is_exit = false;
static bool m_is_connected = false;
static char m_active_remote_ip[25] = "0.0.0.0";
static int m_active_fd = -1;
static char m_buffer[256];
static int m_buff_len = 0;
static char *m_args[MAX_NUM_ARGS];
static int m_nr_args = 0;
static struct local_cmd m_local_cmds[NUM_INTERNAL_CMDS];
static struct remote_cmd m_remote_cmds[MAX_REMOTE_CMDS];
static void *m_remote_cmd_tree;
static int m_udp_fd;
static struct remote_host_info m_remote_hosts[MAX_NUM_REMOTE_HOSTS];
static pthread_mutex_t m_remote_hosts_mtx;
static pthread_t m_udp_thread_id;
static pthread_mutex_t m_udp_thread_mtx;
static void *m_remote_host_tree;
static struct history_cmd_queue m_hist_cmd_queue;
static uint16_t m_num_hist_cmd = 0;



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
static int compare_cmd_name_in_remotecmd_tree(const void *pa, const void *pb);
static int compare_remotecmd_in_remotecmd_tree(const void *pa, const void *pb);
static bool handle_receive_broadcast_msg(int sockfd);
static void add_new_cmd_to_history_queue(char *cmd);
static void destroy_history_queue(struct history_cmd_queue *hist_queue);
static bool connect_to_remote_host_via_index(int index);
static bool send_get_list_cmd_request(int sockfd);
static int recv_data(int sockfd, void *rx_buff, int nr_bytes_to_read);
static bool receive_get_list_cmd_reply(int sockfd);
static bool handle_receive_get_list_cmd_reply(int sockfd, struct ethtcp_header *header);
static bool setup_remote_cmds_list(void);
static void do_nothing(void *tree_node_data);
static bool send_exe_cmd_request(int sockfd);
static bool receive_exe_cmd_reply(int sockfd);
static bool handle_receive_exe_cmd_reply(int sockfd, struct ethtcp_header *header);

static bool local_help(char **args);
static bool local_exit(char **args);
static bool local_history(char **args);
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

	if(!setup_local_cmds() || !setup_remote_cmds_list())
	{
		printf("Failed to set local shell commands!\n");
		exit(EXIT_FAILURE);
	}

	m_hist_cmd_queue.head = NULL;
	m_hist_cmd_queue.tail = NULL;

	setup_udp_server();
	setup_udp_thread();

	printf ("Starting new shell...\n");
	printf ("\n");

	int index = 0;
	while(!m_is_exit)
	{
		if(m_is_connected)
		{
			printf ("%s:%hu$ ", m_active_remote_ip, TCP_CLID_PORT);
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
				struct remote_cmd **iter;
				iter = tfind(m_args[0], &m_remote_cmd_tree, compare_cmd_name_in_remotecmd_tree);
				if(iter == NULL)
				{
					printf("Unknown command: %s!\n", m_args[0]);
				} else
				{
					printf("Executing remote command %s...\n", m_args[0]);
					send_exe_cmd_request(m_active_fd);

					receive_exe_cmd_reply(m_active_fd);
				}
			}
		}

		for(int i = 0; i < m_nr_args; i++)
		{
			free(m_args[i]);
			m_args[i] = NULL;
		}
	}

	destroy_history_queue(&m_hist_cmd_queue);

	if(m_active_fd != -1)
	{
		close(m_active_fd);
		m_active_fd = -1;
	}

	for(int i = 0; i < MAX_REMOTE_CMDS; i++)
	{
		if(m_remote_cmds[i].cmd[0] != '\0')
		{
			tdelete(m_remote_cmds[i].cmd, &m_remote_cmd_tree, compare_cmd_name_in_remotecmd_tree);

			m_remote_cmds[i].cmd[0] = '\0';
			m_remote_cmds[i].description[0] = '\0';
		}
	}

	tdestroy(m_remote_cmd_tree, do_nothing);

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

		if(m_buff_len > 0) 
		{
			add_new_cmd_to_history_queue(buff);
		}
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
	bool start_with_one_minus = false;
	bool start_with_two_minus = false;
	bool start_with_int = false;
	bool is_single_flag = false;
	int i = 0;
	while(*str && !strchr(" \t", *str))
	{
		if(*str == '-')
		{
			/* Token as a single flag or negative interger, for example, -D, -a, -f, -99, -22,... */
			if(i == 0)
			{
				start_with_one_minus = true;
				token[i] = *str++;
				i++;
				nr_chars++;
				continue;
			}

			/* Token as a complete flag, for example, --daemon, --unittest,... */
			if(start_with_one_minus && i == 1)
			{
				start_with_one_minus = false;
				start_with_two_minus = true;
				token[i] = *str++;
				i++;
				nr_chars++;
				continue;
			}		

			/* Token as a complete word-concatenated flag, for example, --add-missing, --frequency-overlap,... */
			/* If there were a token starting with two minus, not allowed to have the 3rd minus right then */
			if(start_with_two_minus && i != 2)
			{
				token[i] = *str++;
				i++;
				nr_chars++;
				continue;
			}

			printf("Invalid argurment with minuses '-'!\n");
			free(token);
			return -1;
		} else if((*str >= '0' && *str <= '9'))
		{
			/* If there were a token starting with two minus, not allowed to have digit right after the two minus */
			/* Or if there were a token starting with one minus, but not negative interger (instead, it's a single flag like -D, -a, -v,...), no more than two characters accepted */
			if((start_with_two_minus && i == 2) || (start_with_one_minus && is_single_flag))
			{
				printf("Invalid argurment with digits '0-9'!\n");
				free(token);
				return -1;
			}

			/* If there were a token starting with a digit, not allowed to have '-' or alphabets right then */
			if(i == 0)
			{
				start_with_int = true;
			}

			token[i] = *str++;
			i++;
			nr_chars++;
		} else if((*str >= 'a' && *str <= 'z') || (*str >= 'A' && *str <= 'Z')) 
		{
			/* Or if there were a token starting with one minus, but not negative interger (instead, it's a single flag like -D, -a, -v,...), no more than two characters accepted */
			/* Or if there were a token starting with a digit, not allowed to have alphabets right then */
			if((start_with_one_minus && i != 1) || (start_with_int))
			{
				printf("Invalid argurment with alphabets 'a-z' or 'A-Z'!\n");
				free(token);
				return -1;
			} else if(start_with_one_minus && i == 1)
			{
				is_single_flag = true;
			}

			token[i] = *str++;
			i++;
			nr_chars++;
		} else if(*str == '.')
		{
			/* Or if there were a token starting with a digit, only floating point '.' is accepted */
			/* Or if there were a token starting with a minus '-', floating point '.' is also accepted */
			if(start_with_int || (start_with_one_minus && !is_single_flag))
			{
				token[i] = *str++;
				i++;
				nr_chars++;
			} else
			{
				printf("Invalid argurment with dots '.'!\n");
				free(token);
				return -1;
			}
		} else
		{
			/* Any characters except for 0-9, a-z, A-Z and '-' are not allowed */
			printf("Invalid argurment, unknown character: '%c'\n", *str);
			free(token);
			return -1;
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
	char *token = malloc(MAX_ARG_LENGTH);
	
	int i = 0;
	int nr_chars = 0;
	while((nr_chars = get_token(cmd, token)) > 0)
	{
		args[i] = token;
		// printf("Token %d: \"%s\"\n", i, token);
		token = malloc(MAX_ARG_LENGTH);
		cmd += nr_chars;
		i++;
	}

	if(nr_chars == -1)
	{
		/* If any invalid argument, must free the previous valid ones */
		for(int j = 0; j < i; j++)
		{
			free(args[j]);
			args[j] = NULL;
		}
		return 0;
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

	strcpy(m_local_cmds[2].cmd, "history");
	m_local_cmds[2].handler = &local_history;
	strcpy(m_local_cmds[2].description, "List all history commands in current shell session.");
	strcpy(m_local_cmds[2].syntax, "history");

	strcpy(m_local_cmds[3].cmd, "scan");
	m_local_cmds[3].handler = &local_scan;
	strcpy(m_local_cmds[3].description, "Scan remote devices being active.");
	strcpy(m_local_cmds[3].syntax, "scan");
	
	strcpy(m_local_cmds[4].cmd, "connect");
	m_local_cmds[4].handler = &local_connect;
	strcpy(m_local_cmds[4].description, "Connect to remote device via an index returned by scan command.");
	strcpy(m_local_cmds[4].syntax, "connect <index>");
	
	strcpy(m_local_cmds[5].cmd, "disconnect");
	m_local_cmds[5].handler = &local_disconnect;
	strcpy(m_local_cmds[5].description, "Disconnect from an already connected remote device.");
	strcpy(m_local_cmds[5].syntax, "disconnect");

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
	for(int i = 0; i < MAX_REMOTE_CMDS; i++)
	{
		if(m_remote_cmds[i].cmd[0] != '\0')
		{
			printf("%-64s %-128s\n", m_remote_cmds[i].cmd, m_remote_cmds[i].description);
		}
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

	printf("Exiting the shell...\n");

	if(m_nr_args > 1)
	{
		printf("exit: Too many arguments!\n\n");
		return false;
	}

	printf("Exited the shell successfully!\n");

	m_is_exit = true;
	return true;
}

static bool local_history(char **args)
{
	(void)args;

	if(m_nr_args > 1)
	{
		printf("history: Too many arguments!\n\n");
		return false;
	}

	printf("%-3s %-128s\n", "Index", "Command");
	printf("%-3s %-128s\n", "-----", "-------");
	struct history_cmd *iter = m_hist_cmd_queue.head;
	for(int i = 0; i < m_num_hist_cmd; i++)
	{
		char index[5];
		sprintf(index, "%d", i + 1);
		printf("%-5s %-128s\n", index, iter->cmd);
		iter = iter->next;
	}
	printf("\n");

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

	printf("%-10s %-64s %-25s %-7s\n", "Unique ID", "Hostname", "IP Address", "Port");
	printf("%-10s %-64s %-25s %-7s\n", "---------", "--------", "----------", "----");
	MUTEX_LOCK(&m_remote_hosts_mtx);
	for(int i = 0; i < MAX_NUM_REMOTE_HOSTS; i++)
	{
		if(strcmp(m_remote_hosts[i].hostname, "") != 0)
		{
			char index[5];
			char port[7];
			sprintf(index, "%d", i + 1);
			sprintf(port, "%hu", TCP_CLID_PORT);
			printf("%-10s %-64s %-25s %-7s\n", index, m_remote_hosts[i].hostname, m_remote_hosts[i].ip, port);
		}
	}
	MUTEX_UNLOCK(&m_remote_hosts_mtx);
	printf("\n");

	return true;
}

static bool local_connect(char **args)
{
	// (void)args;
	
	if(m_nr_args != 2)
	{
		printf("scan: Too few or many arguments!\n\n");
		return false;
	}

	int index = atoi(args[1]);

	if(index != 0)
	{
		if(!connect_to_remote_host_via_index(index - 1)) // Index counted from 1
		{
			return false;
		}
	} else
	{
		printf("scan: Unknown argument %s!\n\n", args[1]);
		return false;
	}

	printf("\n");
	m_is_connected = true;
	return true;
}

static bool local_disconnect(char **args)
{
	(void)args;
	printf("Disconnecting from remote device...\n");
	printf("\n");
	m_is_connected = false;
	
	if(m_active_fd != -1)
	{
		close(m_active_fd);
		m_active_fd = -1;
	}

	for(int i = 0; i < MAX_REMOTE_CMDS; i++)
	{
		if(m_remote_cmds[i].cmd[0] != '\0')
		{
			tdelete(m_remote_cmds[i].cmd, &m_remote_cmd_tree, compare_cmd_name_in_remotecmd_tree);

			m_remote_cmds[i].cmd[0] = '\0';
			m_remote_cmds[i].description[0] = '\0';
		}
	}

	tdestroy(m_remote_cmd_tree, do_nothing);

	printf("Disconnected from remote device successfully!\n");
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

	char hostname[MAX_HOST_NAME_LENGTH];
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
		m_remote_hosts[i].hostname[0] = '\0';
		strcpy(m_remote_hosts[i].ip, "0.0.0.0");
		m_remote_hosts[i].alive_timer_fd = -1;
	}
	MUTEX_UNLOCK(&m_remote_hosts_mtx);

	return true;
}

static bool setup_remote_cmds_list(void)
{
	for(int i = 0; i < MAX_REMOTE_CMDS; i++)
	{
		m_remote_cmds[i].cmd[0] = '\0';
		m_remote_cmds[i].description[0] = '\0';
	}

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

static int compare_cmd_name_in_remotecmd_tree(const void *pa, const void *pb)
{
	const char *cmd_name = pa;
	const struct remote_cmd *remote_cmd = pb;

	return strcmp(cmd_name, remote_cmd->cmd);
}

static int compare_remotecmd_in_remotecmd_tree(const void *pa, const void *pb)
{
	const struct remote_cmd *remote_cmda = pa;
	const struct remote_cmd *remote_cmdb = pb;
	
	return strcmp(remote_cmda->cmd, remote_cmdb->cmd);
}

static void add_new_cmd_to_history_queue(char *cmd)
{
	struct history_cmd *new_cmd = (struct history_cmd *)malloc(sizeof(struct history_cmd));
	new_cmd->next = NULL;
	strcpy(new_cmd->cmd, cmd);

	/* Have not had any history command yet */
	if(m_num_hist_cmd == 0)
	{
		new_cmd->prev = NULL;
		m_hist_cmd_queue.head = new_cmd;
		m_hist_cmd_queue.tail = new_cmd;
		m_num_hist_cmd++;
	} else if(m_num_hist_cmd == MAX_HISTORY_CMDS)
	{
		/* Pop the first element from the queue to have slot for new cmd */
		m_hist_cmd_queue.head = m_hist_cmd_queue.head->next;
		free(m_hist_cmd_queue.head->prev);

		m_hist_cmd_queue.tail->next = new_cmd;
		new_cmd->prev = m_hist_cmd_queue.tail;
		m_hist_cmd_queue.tail = new_cmd;
	} else
	{
		m_hist_cmd_queue.tail->next = new_cmd;
		new_cmd->prev = m_hist_cmd_queue.tail;
		m_hist_cmd_queue.tail = new_cmd;
		m_num_hist_cmd++;
	}
}

static void destroy_history_queue(struct history_cmd_queue *hist_queue)
{
	if(hist_queue == NULL)
	{
		printf("Failed to destroy_history_queue due to history queue is NULL!\n");
		return;
	}

	struct history_cmd *iter = hist_queue->head;
	for(;iter != NULL;)
	{
		struct history_cmd *tmp = iter->next;
		free(iter);
		iter = tmp;
	}
}

static bool connect_to_remote_host_via_index(int index)
{
	if(index < 0 || index > MAX_NUM_REMOTE_HOSTS - 1)
	{
		printf("Index %d out of range!\n", index);
		return false;
	} else if(strcmp(m_remote_hosts[index].ip, "0.0.0.0") == 0)
	{
		printf("Remote host for index %d not found in scan table!\n", index);
		return false;
	}

	printf ("Connecting to device...\n");

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		printf("Failed to get socket(), errno = %d!\n", errno);
		return false;
	}

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(m_remote_hosts[index].ip);
	serveraddr.sin_port = htons(TCP_CLID_PORT);

	int res = connect(sockfd, (struct sockaddr *)((void *)&serveraddr), sizeof(struct sockaddr_in));
	if(res < 0)
	{
		printf("Failed to get connect(), errno = %d!\n", errno);
		close(sockfd);
		return false;
	}

	strcpy(m_active_remote_ip, m_remote_hosts[index].ip);
	m_active_fd = sockfd;
	printf("Connected to device: tcp://%s:%d\n", m_remote_hosts[index].ip, TCP_CLID_PORT);

	if(!send_get_list_cmd_request(sockfd))
	{
		return false;
	}

	if(!receive_get_list_cmd_reply(sockfd))
	{
		return false;
	}

	return true;
}

static bool send_get_list_cmd_request(int sockfd)
{
	size_t msg_len = offsetof(struct ethtcp_msg, payload) + sizeof(struct clid_get_list_cmd_request);
	struct ethtcp_msg *req = malloc(msg_len);
	if(req == NULL)
	{
		printf("Failed to malloc locate mbox request message!\n");
		return false;
	}

	uint32_t payload_length = sizeof(struct clid_get_list_cmd_request);
	req->header.sender 					= htonl((uint32_t)getpid());
	req->header.receiver 					= htonl(111);
	req->header.protRev 					= htonl(15);
	req->header.msgno 					= htonl(CLID_GET_LIST_CMD_REQUEST);
	req->header.payloadLen 					= htonl(payload_length);

	req->payload.clid_get_list_cmd_request.errorcode	= htonl(CLID_STATUS_OK);

	int res = send(sockfd, req, msg_len, 0);
	if(res < 0)
	{
		printf("Failed to send CLID_GET_LIST_CMD_REQUEST, errno = %d!\n", errno);
		return false;
	}

	free(req);
	printf("Sent CLID_GET_LIST_CMD_REQUEST successfully!\n");
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

static bool receive_get_list_cmd_reply(int sockfd)
{
	struct ethtcp_header *header;
	int header_size = sizeof(struct ethtcp_header);
	char rxbuff[header_size];
	int size = 0;

	size = recv_data(sockfd, rxbuff, header_size);

	if(size == 0)
	{
		printf("Clid from this fd %d just disconnected!\n", sockfd);
		return true;
	} else if(size < 0)
	{
		printf("Receive data from this clid failed, fd = %d!\n", sockfd);
		return false;
	}

	header = (struct ethtcp_header *)rxbuff;
	header->msgno 			= ntohl(header->msgno);
	header->payloadLen 		= ntohl(header->payloadLen);
	header->protRev			= ntohl(header->protRev);
	header->receiver		= ntohl(header->receiver);
	header->sender			= ntohl(header->sender);

	printf("Receiving %d bytes from fd %d\n", size, sockfd);
	printf("Re-interpret TCP packet: msgno: 0x%08x\n", header->msgno);
	printf("Re-interpret TCP packet: payloadLen: %u\n", header->payloadLen);
	printf("Re-interpret TCP packet: protRev: %u\n", header->protRev);
	printf("Re-interpret TCP packet: receiver: %u\n", header->receiver);
	printf("Re-interpret TCP packet: sender: %u\n", header->sender);

	switch (header->msgno)
	{
	case CLID_GET_LIST_CMD_REPLY:
		printf("Received CLID_GET_LIST_CMD_REQUEST!\n");
		handle_receive_get_list_cmd_reply(sockfd, header);
		break;
	
	default:
		printf("Received unknown TCP packet, drop it!\n");
		break;
	}

	return true;
}

static bool handle_receive_get_list_cmd_reply(int sockfd, struct ethtcp_header *header)
{
	struct clid_get_list_cmd_reply *rep;
	uint32_t payloadLen = header->payloadLen;
	char rxbuff[payloadLen];
	int size = 0;

	size = recv_data(sockfd, rxbuff, payloadLen);

	if(size <= 0)
	{
		printf("Failed to receive data from this clid, fd = %d!\n", sockfd);
		return false;
	}

	rep = (struct clid_get_list_cmd_reply *)rxbuff;
	rep->errorcode 			= ntohl(rep->errorcode);
	rep->payload_length		= ntohl(rep->payload_length);

	printf("Receiving %d bytes from fd %d\n", size, sockfd);
	printf("Re-interpret TCP packet: errorcode: %u\n", rep->errorcode);
	printf("Re-interpret TCP packet: payload_length: %u\n", rep->payload_length);

	unsigned long offset = 2;
	uint16_t cmd_len = 0;
	char cmd_buff[MAX_ARG_LENGTH];
	uint16_t num_cmds = *((uint16_t *)(&rep->payload));
	printf("Re-interpret TCP packet: num_cmds: %hu\n", num_cmds);
	for(int i = 0; i < num_cmds; i++)
	{
		int j = 0;
		for(; j < MAX_REMOTE_CMDS; j++)
		{
			if(m_remote_cmds[j].cmd[0] == '\0')
			{
				/* This is command name */
				cmd_len = *((uint16_t *)(&rep->payload + offset));
				offset += 2;
				memcpy(cmd_buff, (&rep->payload + offset), cmd_len);
				cmd_buff[cmd_len] = '\0';
				offset += cmd_len;
				printf("Re-interpret TCP packet: cmd_len %d: %hu\n", i, cmd_len);
				printf("Re-interpret TCP packet: cmd %d: %s\n", i, cmd_buff);
				strcpy(m_remote_cmds[j].cmd, cmd_buff);

				/* This is command description */
				cmd_len = *((uint16_t *)(&rep->payload + offset));
				offset += 2;
				memcpy(cmd_buff, (&rep->payload + offset), cmd_len);
				cmd_buff[cmd_len] = '\0';
				offset += cmd_len;
				printf("Re-interpret TCP packet: cmd_desc_len %d: %hu\n", i, cmd_len);
				printf("Re-interpret TCP packet: cmd_desc %d: %s\n", i, cmd_buff);
				strcpy(m_remote_cmds[j].description, cmd_buff);

				struct remote_cmd **iter;
				iter = tfind(m_remote_cmds[j].cmd, &m_remote_cmd_tree, compare_cmd_name_in_remotecmd_tree);
				if(iter != NULL)
				{
					printf("Command \"%s\" already added in remote cmd tree, something wrong!\n", m_remote_cmds[j].cmd);
				} else
				{
					tsearch(&m_remote_cmds[j], &m_remote_cmd_tree, compare_remotecmd_in_remotecmd_tree);
				}

				break;
			}
		}

		if(j == MAX_REMOTE_CMDS)
		{
			printf("No more than %d remote cmd can be added!\n", MAX_REMOTE_CMDS);
			return false;
		}
	}
	
	return true;
}

static void do_nothing(void *tree_node_data)
{
	(void)tree_node_data;
}

static bool send_exe_cmd_request(int sockfd)
{
	uint32_t total_len = 0;
	uint16_t cmd_len = 0;
	char cmds_buff[2 + strlen(m_args[0]) + 2 + m_nr_args*(2 + MAX_ARG_LENGTH)];

	cmd_len = strlen(m_args[0]);
	*((uint16_t *)(&cmds_buff[total_len])) = cmd_len;
	total_len += 2;
	strcpy(&cmds_buff[total_len], m_args[0]);
	total_len += cmd_len;

	*((uint16_t *)(&cmds_buff[total_len])) = m_nr_args;
	total_len += 2;

	for(int i = 0; i < m_nr_args; i++)
	{
		cmd_len = strlen(m_args[i]);
		*((uint16_t *)(&cmds_buff[total_len])) = cmd_len;
		total_len += 2;
		strcpy(&cmds_buff[total_len], m_args[i]);
		total_len += cmd_len;
	}

	size_t msg_len = offsetof(struct ethtcp_msg, payload) + offsetof(struct clid_exe_cmd_request, payload) + total_len;
	struct ethtcp_msg *rep = malloc(msg_len);
	if(rep == NULL)
	{
		printf("Failed to malloc locate mbox reply message!\n");
		return false;
	}

	uint32_t payload_length = offsetof(struct clid_exe_cmd_request, payload) + total_len;
	rep->header.sender 					= htonl((uint32_t)getpid());
	rep->header.receiver 					= htonl(111);
	rep->header.protRev 					= htonl(15);
	rep->header.msgno 					= htonl(CLID_EXE_CMD_REQUEST);
	rep->header.payloadLen 					= htonl(payload_length);

	rep->payload.clid_exe_cmd_request.errorcode		= htonl(CLID_STATUS_OK);
	rep->payload.clid_exe_cmd_request.payload_length	= htonl(total_len);
	memcpy(rep->payload.clid_exe_cmd_request.payload, cmds_buff, total_len);

	int res = send(sockfd, rep, msg_len, 0);
	if(res < 0)
	{
		printf("Failed to send CLID_EXE_CMD_REQUEST, errno = %d!\n", errno);
		return false;
	}

	free(rep);
	printf("Sent CLID_EXE_CMD_REQUEST successfully!\n");
	return true;
}

static bool receive_exe_cmd_reply(int sockfd)
{
	struct ethtcp_header *header;
	int header_size = sizeof(struct ethtcp_header);
	char rxbuff[header_size];
	int size = 0;

	size = recv_data(sockfd, rxbuff, header_size);

	if(size == 0)
	{
		printf("Clid from this fd %d just disconnected!\n", sockfd);
		return true;
	} else if(size < 0)
	{
		printf("Receive data from this clid failed, fd = %d!\n", sockfd);
		return false;
	}

	header = (struct ethtcp_header *)rxbuff;
	header->msgno 			= ntohl(header->msgno);
	header->payloadLen 		= ntohl(header->payloadLen);
	header->protRev			= ntohl(header->protRev);
	header->receiver		= ntohl(header->receiver);
	header->sender			= ntohl(header->sender);

	printf("Receiving %d bytes from fd %d\n", size, sockfd);
	printf("Re-interpret TCP packet: msgno: 0x%08x\n", header->msgno);
	printf("Re-interpret TCP packet: payloadLen: %u\n", header->payloadLen);
	printf("Re-interpret TCP packet: protRev: %u\n", header->protRev);
	printf("Re-interpret TCP packet: receiver: %u\n", header->receiver);
	printf("Re-interpret TCP packet: sender: %u\n", header->sender);

	switch (header->msgno)
	{
	case CLID_EXE_CMD_REPLY:
		printf("Received CLID_GET_LIST_CMD_REQUEST!\n");
		handle_receive_exe_cmd_reply(sockfd, header);
		break;
	
	default:
		printf("Received unknown TCP packet, drop it!\n");
		break;
	}

	return true;
}

static bool handle_receive_exe_cmd_reply(int sockfd, struct ethtcp_header *header)
{
	struct clid_exe_cmd_reply *rep;
	uint32_t payloadLen = header->payloadLen;
	char rxbuff[payloadLen];
	int size = 0;

	size = recv_data(sockfd, rxbuff, payloadLen);

	if(size <= 0)
	{
		printf("Failed to receive data from this clid, fd = %d!\n", sockfd);
		return false;
	}

	rep = (struct clid_exe_cmd_reply *)rxbuff;
	rep->errorcode 			= ntohl(rep->errorcode);
	rep->payload_length		= ntohl(rep->payload_length);

	printf("Receiving %d bytes from fd %d\n", size, sockfd);
	printf("Re-interpret TCP packet: errorcode: %u\n", rep->errorcode);
	printf("Re-interpret TCP packet: payload_length: %u\n", rep->payload_length);

	char buff[512];
	memcpy(buff, rep->payload, rep->payload_length);
	buff[rep->payload_length] = '\0';
	
	printf("Re-interpret TCP packet: cmd_output: %s\n", buff);
	printf("\n");

	return true;
}







