#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>


/*****************************************************************************\/
*****                           INTERNAL TYPES                             *****
*******************************************************************************/




/*****************************************************************************\/
*****                         INTERNAL VARIABLES                           *****
*******************************************************************************/
static volatile bool m_is_interrupt = false;
static char m_buffer[256];
static int m_buff_len = 0;
static char *m_args[32];
static int m_nr_args = 0;



/*****************************************************************************\/
*****                   INTERNAL FUNCTIONS PROTOTYPES                      *****
*******************************************************************************/
static void shell_init(void);
static void shell_sig_handler(int signo);
static bool read_input_cmd(char *buff);
static int get_token(const char *str, char *token);
static int get_args(char *cmd, char *args[]);



int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	int opt = 0;
	char tcp_ip[25];
	int port = 0;

	shell_init();

	while((opt = getopt(argc, argv, "ip:")) != -1)
	{
		switch (opt)
		{
		case 'i':
			strcpy(tcp_ip, optarg);
			break;
		
		case 'p':
			port = atoi(optarg);
			break;
		default:
			printf("ERROR: Usage:\t%s\t[-i ip_address]\t[-p port]\n", argv[0]);
			printf("Example:\t%s\t-i   \"192.168.1.2\"\t-p   33333\n", argv[0]);
			exit(EXIT_FAILURE);
			break;
		}
	}

	printf ("Connecting to device...\n");

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		ITC_ERROR("Failed to get socket(), errno = %d!", errno);
		return false;
	}

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(tcp_ip);
	serveraddr.sin_port = htons(port);

	int res = connect(sockfd, (struct sockaddr *)((void *)&serveraddr), sizeof(struct sockaddr_in));
	if(res < 0)
	{
		ITC_ERROR("Failed to get connect(), errno = %d!", errno);
		close(sockfd);
		return false;
	}

	printf ("Connected to device: tcp://%s:%d\n", tcp_ip, port);

	printf ("Opening new shell...\n");
	printf ("\n");

	while(1)
	{
		printf ("shell >> ");

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

		for(int i = 0; i < m_nr_args; i++)
		{
			free(m_args[i]);
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
	m_is_interrupt = true;

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
		if(!m_is_interrupt)
		{
			printf("\nError reading from stdin!\n");
			return false;
		} else
		{
			// printf("\nReceived Ctrl C, just return to the shell!\n");
			m_is_interrupt = false;
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
				printf("get_token - Invalid argurment, found '-' in the middle of argument!\n");
				free(token);
				return 0;
			}
		}

		if((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'z') || (*str >= 'A' && *str <= 'Z'))
		{
			if(is_negative && (*str < '0' || *str > '9'))
			{
				printf("get_token - Invalid argurment, negative numeric argument but found alphabet character!\n");
				free(token);
				return 0;
			}

			token[i] = *str++;
			i++;
			nr_chars++;
		} else
		{
			printf("get_token - Invalid argurment, unknown character: '%c'\n", *str);
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
		printf("Token %d: \"%s\"\n", i, token);
		token = malloc(32);
		cmd += nr_chars;
		i++;
	}

	return i;
}






