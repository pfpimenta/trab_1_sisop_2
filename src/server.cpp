#include <algorithm>
#include <chrono>
#include <csignal>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "../include/packet.hpp"
#include "../include/Row.hpp"
#include "../include/MasterTable.hpp"

MasterTable* masterTable;

pthread_mutex_t termination_signal_mutex = PTHREAD_MUTEX_INITIALIZER;

#define PORT 4000
#define MAX_THREADS 30 // maximum number of threads allowed
#define BUFFER_SIZE 256
#define PAYLOAD_SIZE 128

#define TYPE_CONNECT 0
#define TYPE_FOLLOW 1
#define TYPE_SEND 2
#define TYPE_MSG 3
#define TYPE_ACK 4
#define TYPE_ERROR 5
#define TYPE_DISCONNECT 6

int seqn = 0;

// Global termination flag, set by the signal handler.
bool termination_signal = false;

bool get_termination_signal(){
  pthread_mutex_lock(&termination_signal_mutex);
  bool signal = termination_signal;
  pthread_mutex_unlock(&termination_signal_mutex);
  return signal;
}

void set_termination_signal(){
  pthread_mutex_lock(&termination_signal_mutex);
  termination_signal = true;
  pthread_mutex_unlock(&termination_signal_mutex);
}

//------------------


void closeConnection(int socket, int thread_id)
{
	printf("Closing connection and exiting socket thread: %d\n", thread_id);
	if (shutdown(socket, SHUT_RDWR) !=0 ) {
		std::cout << "Failed to shutdown a connection socket." << std::endl;
	}
	close(socket);
	pthread_exit(NULL);
}

// writes a message in a socket (sends a message through it)
void write_message(int newsockfd, char* message)
{
	/* write in the socket */
    int n;
	n = write(newsockfd, message, strlen(message));
	if (n < 0) 
		printf("ERROR writing to socket");
}

// opens a socket, binds it, and starts listening for incoming connections
int setup_socket()
{
	int sockfd;
    struct sockaddr_in serv_addr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) 
	{
        printf("ERROR creating LISTEN socket");
        exit(-1);
	}

	int enable = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    	printf("setsockopt(SO_REUSEADDR) failed");
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);
    
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) 
	{
		printf("ERROR on binding LISTEN socket");
        exit(-1);
	}

	if (listen(sockfd, 50) != 0) {
		printf("ERROR on activating LISTEN socket");
        exit(-1);
	}

    return sockfd;
}

int accept_connection(int sockfd)
{
	int newsockfd; // socket created for the connection
	socklen_t clilen;
	struct sockaddr_in cli_addr;

	clilen = sizeof(struct sockaddr_in);
	if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1) 
		printf("ERROR on accept");

	return newsockfd;
}

// Thread designated for the connected client
void * socket_thread(void *arg) {
	int socket = *((int *)arg);
	int size = 0;
	int reference_seqn = 0;
	char payload[PAYLOAD_SIZE];
	char client_message[BUFFER_SIZE];
	char buffer[BUFFER_SIZE];
	int message_type = -1;
	int payload_length = -1;
	Row* currentRow;
	std::string currentUser = "not-connected";
	std::string notification;
	
	packet packet_to_send;

	// print pthread id
	pthread_t thread_id = pthread_self();

	int flags = fcntl(socket, F_GETFL);
	if (flags == -1) {
		printf("FAILED getting socket flags.\n");
	}

	flags = flags | O_NONBLOCK;
	if (fcntl(socket, F_SETFL, flags) != 0) {
		printf("FAILED setting socket to NON-BLOCKING mode.\n");
	}
	// zero-fill the buffer
	memset(client_message, 0, sizeof client_message);

	do{
		// receive message
		size = recv(socket, client_message, BUFFER_SIZE-1, 0);
		if (size > 0) {
			memset(payload, 0, sizeof payload); //clear payload buffer

			client_message[size] = '\0';

			// parse socket buffer: get several messages, if there are more than one
			char* token_end_of_packet;
  			char* rest_packet = client_message;
			while((token_end_of_packet = strtok_r(rest_packet, "\n", &rest_packet)) != NULL)
			{
				strcpy(buffer, token_end_of_packet); // put token_end_of_packet in buffer
				char* token;
				char* rest = buffer;
				const char delimiter[2] = ",";

				//seqn
				token = strtok_r(rest, delimiter, &rest);
				reference_seqn = atoi(token);

				//payload_length
				token = strtok_r(rest, delimiter, &rest);
				payload_length = atoi(token);

				//packet_type
				token = strtok_r(rest, delimiter, &rest);
				message_type = atoi(token);

				//payload (get whatever else is in there up to '\n')
				token = strtok_r(rest, "\n", &rest);
				strncpy(payload, token, payload_length);

				switch (message_type) {
					case TYPE_CONNECT:
					{
						std::string username(payload); //copying char array into proper std::string type
						currentUser = username;

						// check if map already has the username in there before inserting
						masterTable->addUserIfNotExists(username);
						std::cout << "User " + currentUser + " is connecting...";

						currentRow = masterTable->getRow(username);
						
						if(currentRow->connectUser())
						{
							std::cout << " connected." << std::endl;
						} else{
							// TODO mandar mensagem pro cliente avisando q ele atingiu o limite
							printf("\n denied: there are already 2 active sessions!\n");
						 	closeConnection(socket, (int)thread_id);
						}
						break;
					}
					case TYPE_FOLLOW:
					{
						std::string newFollowedUsername(payload); //copying char array into proper std::string type
						
						int status = masterTable->followUser(newFollowedUsername, currentUser);
						switch(status){
							case 0:
								// TODO avisar usuario q ta tudo bem
								std::cout << currentUser + " is now following " + newFollowedUsername + "." << std::endl;
								break;
							case -1:
								// user does not exist
								// TODO avisar usuario
								printf("ERROR: user does not exist.");
								break;
							case -2:
								// TODO avisar usuario
								printf("ERROR: user cannot follow himself.");
								break;
							case -3:
								// TODO avisar usuario
								std::cout << currentUser + " is already following " + newFollowedUsername + "." << std::endl;
								break;
						}
						break;
					}
					case TYPE_SEND:
					{
						std::string message(payload); //copying char array into proper std::string type
						masterTable->sendMessageToFollowers(currentUser, message);
						break;
					}
					case TYPE_DISCONNECT:
					{
						// TODO mandar mensagem pro usuario quando ele se desconectar
						currentRow = masterTable->getRow(currentUser);
						currentRow->closeSession();
						std::cout << currentUser + " has disconnected. Session closed. Terminating thread " << (int)thread_id << std::endl;
						if (shutdown(socket, SHUT_RDWR) !=0 ) {
							std::cout << "Failed to gracefully shutdown a connection socket. Forcing..." << std::endl;
						}
						close(socket);
						pthread_exit(NULL);
						break;
					}
				}
			}
		}

		// send message, if there is any
		if(currentUser != "not-connected") // if the user has already connected
		{
			currentRow = masterTable->getRow(currentUser);
			
			if(currentRow->hasNewNotification())
			{
				int activeSessions = currentRow->getActiveSessions();
				if(activeSessions == 1) {
					// consume notification and remove it from the FIFO
					notification = currentRow->popNotification();
					strcpy(payload, notification.c_str());
					packet_to_send = create_packet(payload, 0, seqn);
					seqn++;
					serialize_packet(packet_to_send, buffer);
					write_message(socket, buffer);
				} else if(activeSessions == 2) {
					bool was_notification_delivered = currentRow->get_notification_delivered();
					if(was_notification_delivered) {
						// consume notification and remove it from the FIFO
						notification = currentRow->popNotification();
						strcpy(payload, notification.c_str());
						packet_to_send = create_packet(payload, 0, seqn);
						seqn++;
						serialize_packet(packet_to_send, buffer);
						write_message(socket, buffer);
					} else {
						// consume notification but DO NOT remove it from the FIFO
						notification = currentRow->getNotification();
						strcpy(payload, notification.c_str());
						packet_to_send = create_packet(payload, 0, seqn);
						seqn++;
						serialize_packet(packet_to_send, buffer);
						write_message(socket, buffer);
						currentRow->set_notification_delivered(true);
						bool timeout_condition = true;
						std::time_t start_timestamp = std::time(nullptr);
						while(currentRow->get_notification_delivered() == true && timeout_condition){
							std::time_t loop_timestamp = std::time(nullptr);
							bool timeout_condition = (loop_timestamp - start_timestamp <= 3);
						}
					}
				}
			}
		}
  	}while (get_termination_signal() == false);

	printf("Exiting socket thread: %d\n", (int)thread_id);
	currentRow->closeSession();
	if (shutdown(socket, SHUT_RDWR) !=0 ) {
		std::cout << "Failed to shutdown a connection socket." << std::endl;
	}
	close(socket);
	pthread_exit(NULL);
}

void exit_hook_handler(int signal) {
	std::cout<< std::endl << "Signal received: code is " << signal << std::endl;

	set_termination_signal();
}

int main(int argc, char *argv[])
{
	int i = 0;
	int sockfd;
	int newsockfd;
	int* newsockptr;
	socklen_t clilen;
	char buffer[BUFFER_SIZE];
  	char message[PAYLOAD_SIZE];
	struct sockaddr_in serv_addr, cli_addr;
	packet packet_to_send;
	std::list<pthread_t> threads_list;
	pthread_t newthread;
	std::list<int*> socketptrs_list;

	// Install handler (assign handler to signal)
    std::signal(SIGINT, exit_hook_handler);
	std::signal(SIGTERM, exit_hook_handler);
	std::signal(SIGABRT, exit_hook_handler);

	// load backup table
	masterTable->load_backup_table();

	// setup LISTEN socket
    sockfd = setup_socket();
	printf("Now listening for incoming connections... \n\n");

	clilen = sizeof(struct sockaddr_in);

	// loop that accepts new connections and allocates threads to deal with them
	while(1) {
		
		// If new incoming connection, accept. Then continue as normal.
		if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) != -1) {
			
			if((newsockptr = (int*)malloc(sizeof(int))) == NULL) {
				std::cout << "Failed to allocate enough memory for the newsockptr." << std::endl;
				exit(-1);
			}	
			
			*newsockptr = newsockfd;
			
			if (pthread_create(&newthread, NULL, socket_thread, newsockptr) != 0 ) {
				printf("Failed to create thread\n");
				exit(-1);
			} else {
				threads_list.push_back(newthread);
				socketptrs_list.push_back(newsockptr);
			}
		}

		// Cleanup code for main server thread
		if(get_termination_signal() == true) {
			std::cout << "Server is now gracefully shutting down..." << std::endl;
			// TODO informar aos clientes q o servidor esta desligando
			std::cout << "Closing passive socket..." << std::endl;
			if (shutdown(sockfd, SHUT_RDWR) !=0 ) {
				std::cout << "Failed to shutdown a connection socket." << std::endl;
			}
			close(sockfd);
			for (auto const& i : threads_list) {
				pthread_join(i, NULL);
			}

			std::cout << "Freeing allocated socket pointers memory..." << std::endl;
			for (auto const& i : socketptrs_list) {
				free(i);
			}

			std::cout << "Deleting instantiated objects..." << std::endl;
			masterTable->deleteRows();
			delete(masterTable);

			std::cout << "Shutdown routine successfully completed." << std::endl;
			exit(0);
		}
	}
}
