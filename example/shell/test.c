#define _GNU_SOURCE
#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>


static volatile bool m_is_sigint = false;
static struct termios old_term_settings, current_term_settings;

static void shell_sig_handler(int signo)
{
	(void)signo;
	m_is_sigint = true;

	// printf("\nCalling shell_sig_handler...\n");
}

static void shell_init(void)
{
	// Customize Ctrl-C -> give up and terminate the on-going command -> Return to our shell ready for next commands
	struct sigaction sa;
	sa.sa_handler = &shell_sig_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
}



/* Initialize new terminal i/o settings */
void initTermios(void);
/* Restore old terminal i/o settings */
void resetTermios(void);

/* 
printf("\033[XA"); // Move up X lines;
printf("\033[XB"); // Move down X lines;
printf("\033[XC"); // Move right X column;
printf("\033[XD"); // Move left X column;
printf("\033[2J"); // Clear screen

https://gist.github.com/delameter/b9772a0bf19032f977b985091f0eb5c1
https://web.archive.org/web/20121225024852/http://www.climagic.org/mirrors/VT100_Escape_Codes.html

*/

int main(int argc, char* argv[])
{
	shell_init();

	initTermios();

	char line[1024];
	int i = 0;
	line[0] = '\0';

	char c;
	char arrow_count = 0; // 27 -> 91 -> 65/66/67/68
	char del_count = 0; // 51 -> 126

	while(c = fgetc(stdin))
	{
		if(i > 1023)
		{
			printf("\nLine is too long!\n");
			break;
		}

		switch (c)
		{
		case 27: // Escape sequence character '^['
			arrow_count = 1;
			break;
		case 91:
			if(arrow_count == 1)
			{
				arrow_count = 2;
			}
			break;
		case 65: // Up Arrow
			if(arrow_count == 2)
			{
				// Clear current line
				printf("\33[2K\r");
				line[0] = '\0';
				i = 0;

				printf("GET UP ARROW!");
				strcat(line, "GET UP ARROW!");
				i += strlen(line);
			}
			break;
		case 66: // Down Arrow
			if(arrow_count == 2)
			{
				// Clear current line
				printf("\33[2K\r");
				line[0] = '\0';
				i = 0;

				printf("GET DOWN ARROW!");
				strcat(line, "GET DOWN ARROW!");
				i += strlen(line);
			}
			break;
		case 67: // Right Arrow
			if(arrow_count == 2)
			{
				// printf("GET RIGHT ARROW!");
				if(i < strlen(line))
				{
					printf("\033[1C");
					++i;
				}
				
			}
			break;
		case 68: // Left Arrow
			if(arrow_count == 2)
			{
				// printf("GET LEFT ARROW!");
				if(i > 0)
				{
					printf("\b");
					--i;
				}
			}
			break;
		case 8: // Backspace
			// printf("GET BACKSPACE!");
			if(i > 0)
			{
				// Handle our line[]
				--i;
				line[i] = '\0';
				strcat(line, line + strlen(line) + 1);

				// Clear current line and printf new line
				printf("\33[2K\r");
				printf("%s", line);
				// Move to beginning of the line
				printf("\r");
				// Move right i characters
				for(int iter = 0; iter < i; ++iter)
				{
					printf("\033[1C");
				}
			}
			break;
		case 51:
			del_count = 1;
			break;
		case 126: // Del
			if(del_count == 1)
			{
				if(i < strlen(line))
				{
					// Handle our line[]
					line[i] = '\0';
					strcat(line, line + strlen(line) + 1);

					// Clear current line and printf new line
					printf("\33[2K\r");
					printf("%s", line);
					// Move to beginning of the line
					printf("\r");
					// Move right i characters
					for(int iter = 0; iter < i; ++iter)
					{
						printf("\033[1C");
					}
				}
			}
			break;
		case 10: // Enter
			// Receive command and execute it
			printf("\nExecuting command above!");
			break;
		case 32: // Space
			printf(" ");
			line[i] = c;
			line[i + 1] = '\0';
			++i;
			break;
		case 18: // Ctrl-R

			break;
		case 4: // Pressed Ctrl-D
			break;
		case EOF: // Pressed Ctrl-C
			printf("Received EOF!");
			break;

		default:
			// printf("-%d-", c);
			break;
		}
		
		if(c == 10)
		{
			break;
		}

		if(c == EOF)
		{
			if(m_is_sigint)
			{
				printf("Received Ctrl-C!");
				break;
			}

			break;
		}

		// if(c == 32 || c == 8)
		// {
		// 	continue;
		// }

		printf("[%d:%d]", arrow_count, del_count);
		printf("-%d-", c);
		if(arrow_count == 0 && del_count == 0 && c >= 33 && c <= 125)
		{
			printf("%c", c);
			line[i] = c;
			line[i + 1] = '\0';
			++i;
		}
		
		if(c != 27 && c != 91)
		{
			arrow_count = 0;
		}

		if(c != 51)
		{
			del_count = 0;
		}
	}

	resetTermios();
}




void initTermios(void)
{
	tcgetattr(0, &old_term_settings); /* grab old terminal i/o settings */
	current_term_settings = old_term_settings; /* make new settings same as old settings */
	current_term_settings.c_lflag &= ~(ICANON | ECHO | ECHOE); /* disable buffered i/o */
	tcsetattr(0, TCSANOW, &current_term_settings); /* use these new terminal i/o settings now */
}

void resetTermios(void)
{
	tcsetattr(0, TCSANOW, &old_term_settings);
}