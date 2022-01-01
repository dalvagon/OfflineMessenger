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
#include <signal.h>
#include <stdlib.h>
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
#define MSG_NEW_MSG "   You have new messages:"

extern int errno;	

struct user
{
	char username[40];
	int user_fd, logged, user_tid, message_id;
}users[100];
char MESSAGE[1024], RESPONSE[1024];
struct sockaddr_in server, from;				
int sd, client,  optval = 1, len, bytes, user_count, tid_count = 0;	
FILE* file_help;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t user_tid[100];

char * conv_addr (struct sockaddr_in address);
void signal_handle(int sgn);
static void* treat(void* arg); //functia pentru thread
int f_zero(int x);
int handle_command(int user_fd); // se ocupa de comanda primita
void send_help(int user_fd); // trimite o lista care contine comenzile disponibile utilizatorului
void register_user(int user_fd, char usersame[1024]); // este inregistrat un utilizator
void display_users(int user_fd); // lista utlizatorilor inregistarti
void login_user(int user_fd,char username[1024]); // este logat un utliziator 
void logout_user(int user_fd); // utlizatorul curent este delogat
void delete_user(int user_fd); // utilizatorul curent este este sters de pe server
void send_message(int user_fd, char username[40], char MESSAGE[1024]);
void history_with(int user_fd, char username[40]);


int main ()
{	
	signal(SIGINT, signal_handle);	
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
		perror("[server]Error at read from client\n");
		return 0;
	}
	MESSAGE[bytes] = '\0';

	int i = strlen(MESSAGE) - 1;
	while(MESSAGE[i] == ' ') 
	{
		MESSAGE[i] = '\0';
		i--;
	}

	
	printf("[server][%d][%ld] Message received : %s\n", user_fd, pthread_self(), MESSAGE);
	
	if((strcmp(MESSAGE, "help") == 0) && strlen(MESSAGE) == 4)
	{
		send_help(user_fd);
	}
	else
	if(strstr(MESSAGE, "register") - MESSAGE == 0)
	{
		register_user(user_fd, MESSAGE + 9);
	}
	else
	if(strcmp(MESSAGE, "users") == 0 && strlen(MESSAGE) == 5)
	{
		display_users(user_fd);
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
	if(strstr(MESSAGE, "send to ") - MESSAGE == 0)
	{
		char username[40];
		if(strlen(MESSAGE ))
		strcpy(username, MESSAGE + 8);
		int i = 0;
		while(username[i] != ' ' && username[i] != '\0')
		{
			i++;
		}
		strcpy(username + i, "\0");
		send_message(user_fd, username, MESSAGE + 8 + i);
	}
	else
	if(strstr(MESSAGE, "history with ") - MESSAGE == 0)
	{
		char username[40];
		bzero(username, 40);
		strcat(username, MESSAGE + 13);
		printf("[server]%s\n", username);
		history_with(user_fd, username);
	}
	else
	{
		int len = strlen(MSG_BAD_COMMAND);
		if((write(user_fd, &len, sizeof(int)) <= 0))
		{
			perror("Error at write\n");
		}
		if((write(user_fd, MSG_BAD_COMMAND, strlen(MSG_BAD_COMMAND)) <= 0))
		{
			perror("Error at write\n");
		}
	}
	return 1;
}

void send_help(int user_fd)
{
	char text[1024];
	file_help = fopen("help.txt", "r"); 
	pthread_mutex_lock(&lock);
	while(fgets(text, sizeof(text), file_help) != NULL) 
	{
		strcat(RESPONSE, "   ");
		strcat(RESPONSE, text);
	}
	pthread_mutex_unlock(&lock);
	int len = strlen(RESPONSE);
	if((write(user_fd, &len, sizeof(int)) <= 0))
	{
		perror("Error at write\n");
		
	}
	if((write(user_fd, RESPONSE, strlen(RESPONSE)) <= 0))
	{
		perror("Error at write\n");
	}
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
			int len = strlen(MSG_LOGGED_IN);
			if((write(user_fd, &len, sizeof(int)) <= 0))
			{
				perror("Error at write\n");
			}
			if((write(user_fd, MSG_LOGGED_IN, strlen(MSG_LOGGED_IN)) <= 0))
			{
				perror("Error at write\n");
			}
		}
	}
	if(ok == 1)
	{
		for(int i = 1; i <= user_count && ok; i++)
		{
			if(strcmp(username, users[i].username) == 0)
			{
				ok = 0;
				int len = strlen(MSG_USER_REGISTERED);
				if((write(user_fd, &len, sizeof(int)) <= 0))
				{
					perror("Error at write\n");	
				}
				if((write(user_fd, MSG_USER_REGISTERED, strlen(MSG_USER_REGISTERED)) <= 0))
				{
					perror("Error at write\n");	
				}
			}
		}
		
		if(ok == 1)
		{
			pthread_mutex_lock(&lock);
			if(strlen(username) > 0)
			{
				user_count++;
				bzero(users[user_count].username, 40);
				strcpy(users[user_count].username, username);
				users[user_count].logged = 0;
				users[user_count].message_id = 0;
				sprintf(RESPONSE, "   User \e[1;91m%s \e[1;97mwas registered!", users[user_count].username);
				RESPONSE[strlen(RESPONSE)] = '\0';	

				int len = strlen(RESPONSE);
				if((write(user_fd, &len, sizeof(int)) <= 0))
				{
					perror("Error at write\n");	
				}
				if((write(user_fd, RESPONSE, strlen(RESPONSE)) <= 0))
				{
					perror("Error at write\n");	
				}
			}
			else
			{
				int len = strlen("   Enter an username!");
				if((write(user_fd, &len, sizeof(int)) <= 0))
				{
					perror("Error at write\n");	
				}
				if((write(user_fd, "   Enter an username!", strlen("   Enter an username!")) <= 0))
				{
					perror("Error at write\n");	
				}
			}
			pthread_mutex_unlock(&lock);
		}
	}
}

void login_user(int user_fd, char username[1024])
{
	int ok = 0;
	for(int i = 1; i <= user_count && !ok; i++)
	{
		if(user_fd == users[i].user_fd && users[i].logged == 1)
		{
			int len = strlen(MSG_USER_ALREADY_LOGGED_IN);
			if((write(user_fd, &len, sizeof(int)) <= 0))
			{
				perror("Error at write\n");
			}
			if((write(user_fd, MSG_USER_ALREADY_LOGGED_IN, strlen(MSG_USER_ALREADY_LOGGED_IN)) <= 0))
			{
				perror("Error at write\n");
			}
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
					int len = strlen(MSG_USER_ALREADY_LOGGED_IN);
					if((write(user_fd, &len, sizeof(int)) <= 0))
					{
						perror("Error at write\n");
					}
					if((write(user_fd, MSG_USER_ALREADY_LOGGED_IN, strlen(MSG_USER_ALREADY_LOGGED_IN)) <= 0))
					{
						perror("Error at write\n");		
					}
				}
				else
				{
					char path_q[100], message[1024];
					FILE* file_path_q;
					users[i].logged = 1;
					users[i].user_fd = user_fd;
					
					printf("[server]User %s was logged in\n", username);
					
					sprintf(path_q, "q%s.txt", username);
					if(access(path_q, F_OK | R_OK) == 0)
					{
						if((file_path_q = fopen(path_q, "r")) != NULL)
						{
							int len = 0;

							sprintf(RESPONSE, "%s \e[0;92m%s\e[1;97m", MSG_USER_LOGGED_IN, users[i].username);
							len = strlen(RESPONSE);
							
							if((write(user_fd, &len, sizeof(int)) <= 0))
							{
								perror("Error at write\n");
							}

							if((write(user_fd, RESPONSE, strlen(RESPONSE)) <= 0))
							{
								perror("Error at write\n");			
							}

							len = 0;

							while(fgets(message, sizeof(message), file_path_q) != NULL)
							{
								len+= strlen(message);
							}
							fclose(file_path_q);

							bzero(RESPONSE, 1024);
							strcat(RESPONSE, MSG_NEW_MSG);
							len+= strlen(MSG_NEW_MSG);

							if((write(user_fd, &len, sizeof(int)) <= 0))
							{
								perror("Error at write\n");
							}

							if((write(user_fd, RESPONSE, strlen(RESPONSE)) <= 0))
							{
								perror("Error at write\n");			
							}
							file_path_q = fopen(path_q, "r");

							while(fgets(message, sizeof(message), file_path_q) != NULL)
							{
								printf("[server]queue message: %s\n", message);
								if((write(user_fd, message, strlen(message)) <= 0))
								{
									perror("Error at write\n");					
								}
							}

							fclose(file_path_q);
							remove(path_q);
						}
						else
						{
							perror("Error at fopen\n");
						}
					}
					else
					{
						sprintf(RESPONSE, "%s \e[0;92m%s\e[1;97m", MSG_USER_LOGGED_IN, users[i].username);

						int len = strlen(RESPONSE);
						if((write(user_fd, &len, sizeof(int)) <= 0))
						{
							perror("Error at write\n");
						}
						if((write(user_fd, RESPONSE, strlen(RESPONSE)) <= 0))
						{
							perror("Error at write\n");			
						}
					}
					
				}
			}
	}
	if(ok == 0)
	{
		int len = strlen(MSG_BAD_USERNAME);
		if((write(user_fd, &len, sizeof(int)) <= 0))
		{
			perror("Error at write\n");
		}
		if((write(user_fd, MSG_BAD_USERNAME, strlen(MSG_BAD_USERNAME)) <= 0))
		{
			perror("Error at write\n");
			
		}
	}
}

void display_users(int user_fd)
{
	if(user_count == 0)
	{
		int len = strlen(MSG_NO_USER);
		if((write(user_fd, &len, sizeof(int)) <= 0))
		{
			perror("Error at write\n");
		}
		if((write(user_fd, MSG_NO_USER, strlen(MSG_NO_USER)) <= 0))
		{
			perror("Error at write\n");
		}
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
		int len = strlen(RESPONSE);
		if((write(user_fd, &len, sizeof(int)) <= 0))
		{
			perror("Error at write\n");
		}

		if((write(user_fd, RESPONSE, strlen(RESPONSE)) <= 0))
		{
			perror("Error at write\n");
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
				users[i].user_fd = 0;
				ok = 1;
				printf("[server][%d][%ld] User logged out\n", user_fd, pthread_self());
				int len = strlen(MSG_LOGGED_OUT);
				if((write(user_fd, &len, sizeof(int)) <= 0))
				{
					perror("Error at write\n");
				}
				if((write(user_fd, MSG_LOGGED_OUT, strlen(MSG_LOGGED_OUT)) <= 0))
				{
					perror("Error at write\n");}
			}
		}
	}
	if(ok == 0)
	{
		int len = strlen(MSG_NOT_LOGGED);
		if((write(user_fd, &len, sizeof(int)) <= 0))
		{
			perror("Error at write\n");
		}
		if((write(user_fd, MSG_NOT_LOGGED, strlen(MSG_NOT_LOGGED)) <= 0))
		{
			perror("Error at write\n");
		}
	}
}

void delete_user(int user_fd)
{
	int ok = 0, index = 0;
	for(int i = 1; i <= user_count && !ok; i++)
	{
		if(users[i].user_fd == user_fd)
		{
			index = i;
			if(users[i].logged == 1)
			{
				users[i].logged = 0;
				ok = 1;
				int len = strlen(MSG_DELETED);
				if((write(user_fd, &len, sizeof(int)) <= 0))
				{
					perror("Error at write\n");
				}
				if((write(user_fd, MSG_DELETED, strlen(MSG_DELETED)) <= 0))
				{
					perror("Error at write\n");}
			}
		}
	}

	if(ok == 0)
	{
		int len = strlen(MSG_NOT_LOGGED);
		if((write(user_fd, &len, sizeof(int)) <= 0))
		{
			perror("Error at write\n");
		}
		if((write(user_fd, MSG_NOT_LOGGED, strlen(MSG_NOT_LOGGED)) <= 0))
		{
			perror("Error at write\n");
		}
	}
	
	if(index != 0)
	{
		for(int i = 1; i <= user_count; i++)
		{
			char path[100];
			if(strcmp(users[index].username, users[i].username) < 0)
				sprintf(path, "%s%s.txt", users[index].username, users[i].username);
			else
				sprintf(path, "%s%s.txt", users[i].username, users[index].username);
			printf("[server]file to delete: %s\n", path);
			if(access(path, F_OK) == 0)
			{
				printf("[server]file to delete: %s\n", path);
				remove(path);
			}
			sprintf(path, "q%s.txt", users[i].username);
			if(access(path, F_OK) == 0)
			{
				printf("[server]file to delete: %s\n", path);
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
	int ok = 0, i, destination_fd, ok_logged = 0, index_sender;
	char sender[40], message[1024];
	bzero(sender, 40);
	for(i = 1; i <= user_count && !ok_logged; i++)
	{
		if(user_fd == users[i].user_fd)
		{
			if(users[i].logged == 1)
			{
				ok_logged = 1;
				index_sender = i;
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
				printf("[server]Sending to %s\n", username);
				destination_fd = users[i].user_fd;
				ok = 1;
			}
		}
		if(ok == 1)
		{
			if(destination_fd != user_fd)
			{
				char path_history_with[100];
				FILE* file_path_history_with;
				if(strcmp(sender, username) < 0)
					sprintf(path_history_with, "%s%s.txt", sender, username);
				else
					sprintf(path_history_with, "%s%s.txt", username, sender);

				pthread_mutex_lock(&lock);

				if((file_path_history_with = fopen(path_history_with, "a")) == NULL)
				{
					perror("Error at fopen()");
				}
			
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

				printf("[server][%d][%ld]from %d to %d message %s\n", user_fd, pthread_self(), user_fd, destination_fd, MESSAGE);
				

				int ID = users[index_sender].message_id * f_zero(users[index_sender].user_fd) + users[index_sender].user_fd;
				users[index_sender].message_id++;

				bzero(message, 1024);
				sprintf(message,"   \e[45m[%d][%s][%d:%d:%d]\e[0m \e[1;96m%s\e[0m", ID, sender, hour, minute, second, MESSAGE);
				message[strlen(message)] = '\0';
				fprintf(file_path_history_with, "   \e[1;97m\e[45m[%d][%s][%d:%d:%d]\e[0m \e[1;96m%s\e[0m\n", ID, sender, hour, minute, second, MESSAGE);
				
				strcat(RESPONSE, " ->");
				strcat(RESPONSE, message);
				
				int len = strlen(RESPONSE);
				if((write(user_fd, &len, sizeof(int)) <= 0))
				{
					perror("Error at write\n");
				}
				if((write(user_fd, RESPONSE, strlen(RESPONSE)) <= 0))
				{
					perror("Error at write\n");
				}

				fclose(file_path_history_with);
				
				pthread_mutex_unlock(&lock);

				if(users[i - 1].logged == 1)
				{
					int len = strlen(message);
					pthread_mutex_lock(&lock);
					printf("[server]%d\n", len);
					if((write(destination_fd, &len, sizeof(int)) <= 0))
					{
						perror("Error at write\n");
					}
					if((write(destination_fd, message, len) <= 0))
					{
						perror("Error at write\n");
					}
					pthread_mutex_unlock(&lock);
				}
				else
				{
					char path_q[100];
					bzero(path_q, 80);
					FILE* file_path_q;
					sprintf(path_q, "q%s.txt", username);
					pthread_mutex_lock(&lock);
					if((file_path_q = fopen(path_q, "a")) == NULL)
					{
						perror("[server]Error at fopen()\n");
					}
					fprintf(file_path_q, "   \e[1;97m\e[45m[%d][%s][%d:%d:%d]\e[0m \e[1;96m%s\e[0m\n", ID, sender, hour, minute, second, MESSAGE);
					fclose(file_path_q);
					pthread_mutex_unlock(&lock);
				}
			}
			else
			{
				int len = strlen(MSG_SELF_MESSAGE);
				if((write(user_fd, &len, sizeof(int)) <= 0))
				{
					perror("Error at write\n");
				}
				if((write(user_fd, MSG_SELF_MESSAGE, strlen(MSG_SELF_MESSAGE)) <= 0))
				{
					perror("Error at write\n");}
			}
		}
		else
		{
			int len = strlen(MSG_BAD_USERNAME);
			if((write(user_fd, &len, sizeof(int)) <= 0))
			{
				perror("Error at write\n");
			}
			if((write(user_fd, MSG_BAD_USERNAME, strlen(MSG_BAD_USERNAME)) <= 0))
			{
				perror("Error at write\n");
			}
		}
	}
	else
	{
		int len = strlen(MSG_NOT_LOGGED);
		if((write(user_fd, &len, sizeof(int)) <= 0))
		{
			perror("Error at write\n");
		}
		if((write(user_fd, MSG_NOT_LOGGED, strlen(MSG_NOT_LOGGED)) <= 0))
		{
			perror("Error at write\n");
		}
	}
}

void history_with(int user_fd, char username[40])
{
	int ok = 0, i, ok_logged = 0, with_fd;
	char sender[40];
	bzero(sender, 40);
	for(i = 1; i <= user_count && !ok_logged; i++)
	{
		if(user_fd == users[i].user_fd)
		{
			if(users[i].logged == 1)
			{
				ok_logged = 1;
				strcat(sender, users[i].username);
				printf("[server]logged\n");
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
				printf("[server]username ok\n");
			}
		}
		if(ok == 1)
		{
			with_fd = users[i - 1].user_fd;
			if(with_fd != user_fd)
			{
				printf("[server]history\n");
				char path_history_with[100];
				FILE* file_path_history_with;
				if(strcmp(sender, username) < 0)
					sprintf(path_history_with, "%s%s.txt", sender, username);
				else
					sprintf(path_history_with, "%s%s.txt", username, sender);
				printf("[server]%s\n", path_history_with);
				pthread_mutex_lock(&lock);
				if((file_path_history_with = fopen(path_history_with, "r")) != NULL)
				{
					char message[1024];
					int len = 0;
					printf("[server][2]%s\n", path_history_with);
					while(fgets(message, sizeof(message), file_path_history_with) != NULL)
					{
						len+= strlen(message);
					}
					fclose(file_path_history_with);


					if((write(user_fd, &len, sizeof(int)) <= 0))
					{
						perror("Error at write\n");
					}
					file_path_history_with = fopen(path_history_with, "r");
					while(fgets(message, sizeof(message), file_path_history_with) != NULL)
					{
						write(user_fd, message, strlen(message));
						printf("[server]%s[%ld]\n", message, strlen(message));
					}
					fclose(file_path_history_with);
				}
				else
				{
					perror("Error at fopen\n");
					int len = strlen(MSG_NO_HISTORY);
					if((write(user_fd, &len, sizeof(int)) <= 0))
					{
						perror("Error at write\n");
					}
					if((write(user_fd, MSG_NO_HISTORY, strlen(MSG_NO_HISTORY)) <= 0))
					{
						perror("Error at write\n");
					}
				}
				pthread_mutex_unlock(&lock);
			}
			else
			{
				int len = strlen(MSG_SELF_HISTORY);
				if((write(user_fd, &len, sizeof(int)) <= 0))
				{
					perror("Error at write\n");
				}
				if((write(user_fd, MSG_SELF_HISTORY, strlen(MSG_SELF_HISTORY)) <= 0))
				{
					perror("Error at write\n");}
			}
		}
		else
		{
			int len = strlen(MSG_BAD_USERNAME);
			if((write(user_fd, &len, sizeof(int)) <= 0))
			{
				perror("Error at write\n");
			}
			if((write(user_fd, MSG_BAD_USERNAME, strlen(MSG_BAD_USERNAME)) <= 0))
			{
				perror("Error at write\n");
			}
		}
	}
	else
	{
		int len = strlen(MSG_NOT_LOGGED);
		if((write(user_fd, &len, sizeof(int)) <= 0))
		{
			perror("Error at write\n");
		}
		if((write(user_fd, MSG_NOT_LOGGED, strlen(MSG_NOT_LOGGED)) <= 0))
		{
			perror("Error at write\n");
		}
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

void signal_handle(int sgn)
{
	printf("[SERVER]EXIT");
	for(int i = 1 ; i <= user_count; i++)
	{
		delete_user(users[i].user_fd);
	}
	exit(0);
}

int f_zero(int x)
{
	int z = 1;
	while(x)
	{
		x/=10;
		z*=10;
	}
	return z;
}