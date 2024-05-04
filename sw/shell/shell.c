#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

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
	char	cmd[64];
	bool	(*handler)(char **args);
	char	description[128];
	char	syntax[64];
};

#define NUM_INTERNAL_CMDS	5


/*****************************************************************************\/
*****                         INTERNAL VARIABLES                           *****
*******************************************************************************/
static volatile bool m_is_sigint = false;
static bool m_is_exit = false;
static bool m_is_connected = false;
static char m_remote_ip[25] = "0.0.0.0";
static int m_remote_port = -1;
static char m_buffer[256];
static int m_buff_len = 0;
static char *m_args[32];
static int m_nr_args = 0;
struct local_cmd local_cmds[NUM_INTERNAL_CMDS];




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
		printf("Failed to set local shell commands!");
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
	// 	ITC_ERROR("Failed to get socket(), errno = %d!", errno);
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
	// 	ITC_ERROR("Failed to get connect(), errno = %d!", errno);
	// 	close(sockfd);
	// 	return false;
	// }

	// printf ("Connected to device: tcp://%s:%d\n", tcp_ip, port);

	printf ("Starting new shell...\n");
	printf ("\n");

	int index = 0;
	while(!m_is_exit)
	{
		if(m_is_connected)
		{
			printf ("%s:%d$ ", m_remote_ip, m_remote_port);
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
		if(strcmp(str,local_cmds[i].cmd) == 0)
		{
			return i;
		}
		
		i++;
	}

	return -1;
}

void execute_local_cmd(int index, char **args)
{
	if(local_cmds[index].handler == NULL)
	{
		printf("Handler for command %s not found!", local_cmds[index].cmd);
		return;
	}

	local_cmds[index].handler(args);
}

static bool setup_local_cmds(void)
{
	strcpy(local_cmds[0].cmd, "help");
	local_cmds[0].handler = &local_help;
	strcpy(local_cmds[0].description, "List all local and remote commands, respective syntaxes and description.");
	strcpy(local_cmds[0].syntax, "help");

	strcpy(local_cmds[1].cmd, "exit");
	local_cmds[1].handler = &local_exit;
	strcpy(local_cmds[1].description, "Disconnect from remote device and exit current shell.");
	strcpy(local_cmds[1].syntax, "exit");

	strcpy(local_cmds[2].cmd, "scan");
	local_cmds[2].handler = &local_scan;
	strcpy(local_cmds[2].description, "Scan remote devices being active. Timeout in seconds (0 is wait forever).");
	strcpy(local_cmds[2].syntax, "scan <timeout>");
	
	strcpy(local_cmds[3].cmd, "connect");
	local_cmds[3].handler = &local_connect;
	strcpy(local_cmds[3].description, "Connect to remote device via ip address and port.");
	strcpy(local_cmds[3].syntax, "connect <ip> <port>");
	
	strcpy(local_cmds[4].cmd, "disconnect");
	local_cmds[4].handler = &local_disconnect;
	strcpy(local_cmds[4].description, "Disconnect from an already connected remote device.");
	strcpy(local_cmds[4].syntax, "disconnect");

	return true;
}

static bool local_help(char **args)
{
	(void)args;

	if(m_nr_args > 1)
	{
		printf("help: Too many arguments!\n");
		return false;
	}

	printf("%-64s %-128s\n", "Syntax", "Description");
	printf("%-64s %-128s\n", "------", "-----------");
	for(int i = 0; i < NUM_INTERNAL_CMDS; i++)
	{
		printf("%-64s %-128s\n", local_cmds[i].syntax, local_cmds[i].description);
	}
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
	printf("scan: calling this command!\n");
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