#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>

#define PORT 2728

#define MSG_WELCOME "   You have been connected! Type help for list of commands."
#define MSG_NO_USER "   No users registered!"
#define MSG_USER_REGISTERED "   User already registered!"
#define MSG_USER_LOGGED_IN "   You have been logged in!"
#define MSG_USER_ALREADY_LOGGED_IN "   User already logged in!"
#define MSG_BAD_COMMAND "   This command does not exist! Type help for a list of available commands."
#define MSG_BAD_USERNAME "   This username is not registered!"
#define MSG_LOGGED_IN "   You can't do that, you are logged in!"
#define MSG_NOT_LOGGED "   You are not logged in"
#define MSG_LOGGED_OUT "   You have been logged out!"
#define MSG_DELETED "   User has been deleted!"
#define MSG_SENT "   Message was sent"
#define MSG_SELF_MESSAGE "   You can't send a message to yourself"
#define MSG_NO_HISTORY "   You don't have a conversation history with this user"
#define MSG_SELF_HISTORY "   You don't have a conversation history with this yourself"
extern int errno;	

struct user
{
	char username[40];
	int user_fd, logged, user_tid;
}users[100];
char MESSAGE[1024], RESPONSE[1024];
struct sockaddr_in server, from;				
int sd, client,  optval = 1, len, bytes, user_count, tid_count = 0;	
FILE* file_help;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t user_tid[100];

char * conv_addr (struct sockaddr_in address);
static void* treat(void* arg); //functia pentru thread
int handle_command(int user_fd); // se ocupa de comanda primita
void send_help(); // trimite o lista care contine comenzile disponibile utilizatorului
void register_user(int user_fd, char usersame[1024]); // este inregistrat un utilizator
void display_users(); // lista utlizatorilor inregistarti
void login_user(int user_fd,char username[1024]); // este logat un utliziator 
void logout_user(int user_fd); // utlizatorul curent este delogat
void delete_user(int user_fd); // utilizatorul curent este este sters de pe server
void send_message(int user_fd, char username[40], char MESSAGE[1024]);
void history_with(int user_fd, char username[40]);

int main ()
{		

	if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror ("[server] Error at socket().\n");
		return errno;
	}

	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval));
	bzero (&server, sizeof (server));

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl (INADDR_ANY);
	server.sin_port = htons (PORT);

	if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
	{
		perror ("[server] Error at bind().\n");
		return errno;
	}

	if (listen (sd, 5) == -1)
	{
		perror ("[server] Error at listen().\n");
		return errno;
	}

	printf ("[server] Waiting at port %d...\n", PORT);
	fflush (stdout);
	while (1)
	{
		len = sizeof (from);
		bzero (&from, sizeof (from));

		client = accept (sd, (struct sockaddr *) &from, &len);

		if (client < 0)
		{
			perror ("[server] Error at accept().\n");
			continue;
		}
		printf("[server] Client with file descriptor %d connected at adress %s.\n",client, conv_addr (from));	
		if(write(client, MSG_WELCOME, strlen(MSG_WELCOME)) < 0)
		{
			perror("Error at write\n");
			close(client);
		}

		if(pthread_create(&user_tid[tid_count++], NULL, &treat, &client) != 0)
		{
			perror("[server] Error ar pthread_create().\n");
			continue;
		}		
	}	
}			

static void* treat(void* arg)
{
	int user_fd = *((int*) arg);
	pthread_detach(pthread_self());
	while(1)
	{
		if(!handle_command(user_fd))
		{
			close(user_fd);
			break;
		}
	}
	return NULL;
}

int handle_command(int user_fd)
{
	bzero(RESPONSE, 1024);
	bzero(MESSAGE, 1024);
	if((bytes = read(user_fd, MESSAGE, sizeof(MESSAGE))) <= 0) 
	{
		perror("Error at read from client\n");
	}
	MESSAGE[bytes] = '\0';

	if(bytes <= 0)
	{
		printf ("[server][%d][%ld] Client with file decriptor %d disconnected.\n", user_fd, pthread_self(), user_fd);
		close(user_fd);
		logout_user(user_fd);
		return 0;
	}

	int i = strlen(MESSAGE) - 1;
	while(MESSAGE[i] == ' ') 
	{
		MESSAGE[i] = '\0';
		i--;
	}

	printf("[server][%d][%ld] Message received : %s\n", user_fd, pthread_self(), MESSAGE);

	if((strcmp(MESSAGE, "help") == 0) && strlen(MESSAGE) == 4)
	{
		send_help();
	}
	else
	if(strstr(MESSAGE, "register") - MESSAGE == 0)
	{
		register_user(user_fd, MESSAGE + 9);
	}
	else
	if(strcmp(MESSAGE, "users") == 0 && strlen(MESSAGE) == 5)
	{
		display_users();
	}
	else
	if(strstr(MESSAGE, "login") - MESSAGE == 0)
	{
		login_user(user_fd, MESSAGE + 6);
	}
	else
	if(strcmp(MESSAGE, "logout") == 0)
	{
		logout_user(user_fd);
	}
	else
	if(strcmp(MESSAGE, "delete me") == 0)
	{
		delete_user(user_fd);
	}
	else
	if(strstr(MESSAGE, "send to") - MESSAGE == 0)
	{
		char username[40];
		strcpy(username, MESSAGE + 8);
		int i = 0;
		while(username[i] != ' ')
		{
			i++;
		}
		strcpy(username + i, "\0");
		send_message(user_fd, username, MESSAGE + 8 + i);
	}
	else
	if(strstr(MESSAGE, "history with") - MESSAGE == 0)
	{
		char username[40];
		strcpy(username, MESSAGE + 13);
		int i = 0;
		while(username[i] != ' ')
		{
			i++;
		}
		strcpy(username + i, "\0");
		history_with(user_fd, username);
	}
	else
	{
		strcat(RESPONSE, MSG_BAD_COMMAND);
		RESPONSE[strlen(RESPONSE)] = '\0';
	}

	if((write(user_fd, RESPONSE, strlen(RESPONSE)) <= 0))
	{
		perror("Error at write\n");
		return 0;
	}
	return 1;
}

void send_help()
{
	char text[1024];
	file_help = fopen("help.txt", "r"); 
	while(fgets(text, sizeof(text), file_help) != NULL) 
	{
		strcat(RESPONSE, "   ");
		strcat(RESPONSE, text);
	}
	RESPONSE[strlen(RESPONSE)] = '\0';
	printf("[server - r]\n%s\n", RESPONSE);

	fclose(file_help);
}

void register_user(int user_fd, char username[1024])
{
	int ok = 1;
	while(username[0] == ' ')
	{
		for(int i = 0; i < strlen(username) - 1; i++)
			username[i] = username[i+1];
		username[strlen(username) - 1] = '\0';
	}
	for(int i = 1; i <= user_count && ok; i++)
	{
		if(user_fd == users[i].user_fd && users[i].logged == 1)
		{
			ok = 0;
			strcat(RESPONSE, MSG_LOGGED_IN);
		}
	}
	if(ok == 1)
	{
		for(int i = 1; i <= user_count && ok; i++)
		{
			if(strcmp(username, users[i].username) == 0)
			{
				ok = 0;
				strcat(RESPONSE, MSG_USER_REGISTERED);
			}
		}
		
		if(ok == 1)
		{
			if(strlen(username) > 0)
			{
				user_count++;
				bzero(users[user_count].username, 40);
				strcpy(users[user_count].username, username);
				users[user_count].logged = 0;
				sprintf(RESPONSE, "   User \e[1;91m%s \e[1;97mwas registered!", users[user_count].username);
				RESPONSE[strlen(RESPONSE)] = '\0';	
			}
			else
			{
				strcat(RESPONSE, "   Enter an username!");
			}
		}
	}
}

void login_user(int user_fd, char username[1024])
{
	int ok = 0;
	if(user_count < 1)
	{
		strcat(RESPONSE, MSG_NO_USER);
	}
	else
	{
		for(int i = 1; i <= user_count && !ok; i++)
		{
			if(user_fd == users[i].user_fd && users[i].logged == 1)
			{
				strcat(RESPONSE,  MSG_USER_ALREADY_LOGGED_IN);
				ok = 1;
			}
		}
		if(ok == 0)
		{
			for(int i = 1; i <= user_count && !ok; i++)
				if(strcmp(username, users[i].username) == 0)
				{
					ok = 1;
					if(users[i].logged == 1)
					{
						strcat(RESPONSE, MSG_USER_ALREADY_LOGGED_IN);
					}
					else
					{
						users[i].logged = 1;
						users[i].user_fd = user_fd;
						sprintf(RESPONSE, "%s \e[0;92m%s\e[1;97m", MSG_USER_LOGGED_IN, users[i].username);
					}
				}
		}
	}
	if(ok == 0)
	{
		strcat(RESPONSE, MSG_BAD_USERNAME);
	}
}

void display_users()
{
	if(user_count == 0)
	{
		strcat(RESPONSE, MSG_NO_USER);
	}
	else
	{
		for(int i = 1; i <= user_count; i++)
		{
			printf("[server][%d][%ld]%s\n", users[i].user_fd, pthread_self(), users[i].username);
			if(i == user_count)
			{
				if(users[i].logged == 1)
				{
					sprintf(RESPONSE + strlen(RESPONSE),"   \e[0;92m%s\e[0m", users[i].username);
				}
				else
				{
					sprintf(RESPONSE + strlen(RESPONSE),"   \e[0;91m%s\e[0m", users[i].username);
				}
			}
			else
			{
				if(users[i].logged == 1)
				{
					sprintf(RESPONSE + strlen(RESPONSE),"   \e[0;92m%s\n\e[0m", users[i].username);
				}
				else
				{
					sprintf(RESPONSE + strlen(RESPONSE),"   \e[0;91m%s\n\e[0m", users[i].username);
				}
			}
		}
	}
}

void logout_user(int user_fd)
{
	int ok = 0;
	for(int i = 1; i <= user_count && !ok; i++)
	{
		if(users[i].user_fd == user_fd)
		{
			if(users[i].logged == 1)
			{
				users[i].logged = 0;
				ok = 1;
				printf("[server][%d][%ld] User logged out\n", user_fd, pthread_self());
				strcat(RESPONSE, MSG_LOGGED_OUT);
			}
		}
	}
	if(ok == 0)
	{
		strcat(RESPONSE, MSG_NOT_LOGGED);
	}
}

void delete_user(int user_fd)
{
	int ok = 0, index;
	for(int i = 1; i <= user_count && !ok; i++)
	{
		if(users[i].user_fd == user_fd)
		{
			if(users[i].logged == 1)
			{
				users[i].logged = 0;
				ok = 1;
				index = i;
				strcat(RESPONSE, MSG_DELETED);
			}
		}
	}

	if(ok == 0)
	{
		strcat(RESPONSE, MSG_NOT_LOGGED);
	}
	else
	{
		for(int i = 1; i <= user_count; i++)
		{
			char path[40];
			bzero(path, 40);
			if(users[index].user_fd < users[i].user_fd)
				sprintf(path, "h%dwith%d", users[index].user_fd, users[i].user_fd);
			else
				sprintf(path, "h%dwith%d", users[i].user_fd, users[index].user_fd);
			if(access(path, F_OK) != -1)
			{
				remove(path);
			}
		}
		for(int i = index; i < user_count; i++)
		{
			users[i] = users[i+1];
		}
		user_count-- ;
		printf("[server][%d][%ld] User deleted\n", user_fd, pthread_self());
	}
}

void send_message(int user_fd, char username[40], char MESSAGE[1024])
{
	int ok = 0, i, destination_fd, ok_logged = 0;
	char sender[40], message[1024];
	bzero(sender, 40);
	for(i = 1; i <= user_count && !ok_logged; i++)
	{
		if(user_fd == users[i].user_fd)
		{
			if(users[i].logged == 1)
			{
				ok_logged = 1;
			    strcat(sender, users[i].username);
			}
		}
	}

	if(ok_logged == 1)
	{ 
		for(i = 1; i <= user_count && !ok; i++)
		{
			if(strcmp(username, users[i].username) == 0)
			{
				ok = 1;
			}
		}
		if(ok == 1)
		{
			destination_fd = users[i - 1].user_fd;
			if(destination_fd != user_fd)
			{
				char path_history_with[40];
				bzero(path_history_with, 40);
				FILE* file_path_history_with;
				if(user_fd < destination_fd)
					sprintf(path_history_with, "h%dwith%d", user_fd, destination_fd);
				else
					sprintf(path_history_with, "h%dwith%d", destination_fd, user_fd);

				file_path_history_with = fopen(path_history_with, "a");
			
				time_t message_time_date;
				int hour, minute, second;
				time(&message_time_date);
				struct tm *message_time = localtime(&message_time_date);

				hour = message_time->tm_hour;         
				minute = message_time->tm_min;        
				second = message_time->tm_sec;
				char time[1024];
				bzero(time, 1024);
				strcpy(time, ctime(&message_time_date));
				time[strlen(time) - 1] = '\0';

				printf("[server][%d][%ld][%s]from %d to %d message %s\n", user_fd, pthread_self(), time, user_fd, destination_fd, MESSAGE);
				bzero(message, 1024);
				sprintf(message,"\e[47m[%s][%d:%d:%d]\e[0m \e[1;96m%s\e[0m", sender, hour, minute, second, MESSAGE);
				fprintf(file_path_history_with, "[%s][%d:%d:%d]%s\n", sender, hour, minute, second, MESSAGE);
				strcat(RESPONSE, MSG_SENT);
				fclose(file_path_history_with);
				
				if(users[i - 1].logged == 1)
				{
					write(destination_fd, message, strlen(message));
				}
			}
			else
			{
				strcat(RESPONSE, MSG_SELF_MESSAGE);
			}
		}
		else
		{
			strcat(RESPONSE, MSG_BAD_USERNAME);
		}
	}
	else
	{
		strcat(RESPONSE, MSG_NOT_LOGGED);
	}
}

void history_with(int user_fd, char username[40])
{
	int ok = 0, i, with_fd, ok_logged = 0;
	for(i = 1; i <= user_count && !ok_logged; i++)
	{
		if(user_fd == users[i].user_fd)
		{
			if(users[i].logged == 1)
			{
				ok_logged = 1;
			}
		}
	}
	
	if(ok_logged == 1)
	{
		for(i = 1; i <= user_count && !ok; i++)
		{
			if(strcmp(username, users[i].username) == 0)
			{
				ok = 1;
			}
		}
		if(ok == 1)
		{
			with_fd = users[i - 1].user_fd;
			if(with_fd != user_fd)
			{
				printf("[server]");
				char path_history_with[40];
				bzero(path_history_with, 40);
				FILE* file_path_history_with;
				if(user_fd < with_fd)
					sprintf(path_history_with, "h%dwith%d", user_fd, with_fd);
				else
					sprintf(path_history_with, "h%dwith%d", with_fd, user_fd);
				if(access(path_history_with, F_OK) != -1)
				{
					file_path_history_with = fopen(path_history_with, "r");
					char message[1024];
					while(fgets(message, sizeof(message), file_path_history_with) != NULL)
					{
						write(user_fd, message, strlen(message));
					}
					fclose(file_path_history_with);
					strcat(RESPONSE, "?+?+?+?+?+?+?+?+?+?+");
				}
				else
				{
					strcat(RESPONSE, MSG_NO_HISTORY);
				}
			}
			else
			{
				strcat(RESPONSE, MSG_SELF_HISTORY);
			}
		}
		else
		{
			strcat(RESPONSE, MSG_BAD_USERNAME);
		}
	}
	else
	{
		strcat(RESPONSE, MSG_NOT_LOGGED);
	}
}

char * conv_addr (struct sockaddr_in address)
{
	static char str[25];
	char port[7];

	strcpy (str, inet_ntoa (address.sin_addr));	
	bzero (port, 7);
	sprintf (port, ":%d", ntohs (address.sin_port));	
	strcat (str, port);
	return (str);
}