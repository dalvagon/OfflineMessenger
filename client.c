#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

#define MSG_USER_LOGGED_IN "   You have been logged in!"
#define MSG_LOGGED_OUT "   You have been logged out!"
#define MSG_DELETED "   User has been deleted!"

extern int errno;

int port;
int bytes;
int logged = 0;
char username[40];
fd_set readfds,  actfds;
struct timeval tv;
int fd, nfds;

int main (int argc, char *argv[])
{
	int sd;		
	struct sockaddr_in server;	
	char MESSAGE[1024], RESPONSE[1024];	


	if (argc != 3)
	{
		printf ("[client] Syntax: %s <server_adress> <port>\n", argv[0]);
		return -1;
	}

	port = atoi (argv[2]);

	if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror ("[client] Error at socket().\n");
		return errno;
	}
	else
	{
		FD_ZERO (&actfds);		
		FD_SET (sd, &actfds);		

		tv.tv_sec = 1;		
		tv.tv_usec = 0;
		
		nfds = sd;
	}
	

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(argv[1]);
	server.sin_port = htons (port);
	
	if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
	{
		perror ("[client]Error at connect().\n");
		return errno;
	}

	bzero (RESPONSE, 1024);
	bytes = read (sd, RESPONSE, sizeof(RESPONSE));
	RESPONSE[bytes] = '\0';
	printf ("[client]%s\n", RESPONSE);

	bzero(username, 40);


	if(logged == 0)
	{
		printf ("\n\e[1;95m>\e[0m ");
	}
	else
	{
		printf("\n\e[42m[%s]\e[0;90m>\e[0m ", username);
	}
	fflush (stdout);


	FD_SET(0, &actfds);

	while(1)
	{
		bzero (MESSAGE, 1024);
		bzero (RESPONSE, 1024);

		bcopy ((char *) &actfds, (char *) &readfds, sizeof (readfds));

        if (select (nfds+1, &readfds, NULL, NULL, &tv) < 0)
		{
			perror ("[server] Error at select().\n");
			continue;
		}
		
		for (fd = 0; fd <= nfds; fd++)
		{
			if(FD_ISSET(fd, &readfds))
			{
				if(fd == 0)
				{
					bytes = read (0, MESSAGE, sizeof(MESSAGE));
					if(bytes > 1)
					{
						MESSAGE[bytes - 1] = '\0';
						if (write (sd, MESSAGE, strlen(MESSAGE)) <= 0)
						{
							perror ("[client]Error at write() to server.\n");
							return errno;
						}
					}
					else
					{
						if(logged == 0)
						{
							printf ("\e[1;95m>\e[0m ");
						}
						else
						{
							printf("[\e[0;92m%s\e[0m]\e[1;95m>\e[0m ", username);
						}
						fflush(stdout);
					}

				}
				else
				{
					int len;
					if(read(sd, &len, sizeof(int)) <= 0)
					{
						exit(0);
					}
					//printf("[client]%d\n", len);
					while(len > 0)
					{
						if(len > 1024)
						{
							bytes = read(sd, RESPONSE, sizeof(RESPONSE));
							RESPONSE[bytes] = '\0';
						}
						else
						{
							bytes = read(sd, RESPONSE, len);
							RESPONSE[bytes] = '\0';
						}
						len = len - bytes;
						printf("\e[1;97m%s\e[0m", RESPONSE);
						fflush(stdout);
						if(strstr(RESPONSE, MSG_USER_LOGGED_IN) - RESPONSE == 0)
						{
							logged = 1;
							bzero(username, 40);
							strcat(username, RESPONSE + 1 + strlen(MSG_USER_LOGGED_IN));
						}
						if(strcmp(RESPONSE, MSG_LOGGED_OUT) == 0 || strcmp(RESPONSE, MSG_DELETED) == 0)
						{
							logged = 0;
						}
					}
					

					if(logged == 0)
					{
						printf ("\n\e[1;95m>\e[0m ");
					}
					else
					{
						printf("\n[\e[0;92m%s\e[0m]\e[1;95m>\e[0m ", username);
					}
					fflush(stdout);
				}
			}
		}
	}
	close (sd);
}
