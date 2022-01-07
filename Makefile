all:
	gcc client.c -lpthread -o client
	gcc server.c -lpthread -o server
client_: 
	./client 127.0.0.1 2728