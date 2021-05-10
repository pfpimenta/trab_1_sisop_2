#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <thread>
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
#define MAX_TRIES 3 // maximum number of tries to send a notification to a client


typedef struct __communication_params{
  char* type; // BACKUP ou PRIMARY
  int local_clients_listen_port;
  int local_servers_listen_port;
  char* remote_primary_server_ip_address;
  int remote_primary_server_port;
} communication_params;

typedef struct __server {
  int id;
  char* ip;
  int port;
} server_struct;

// TODO mutexes, transformar em classe!
std::map< int*, server_struct* > servers_table;

// TODO mutexes, jogar na classe!
int gid = 0;
int myServerID = -1;

// Global termination flag, set by the signal handler.
bool termination_signal = false;

// Global primary flag
bool is_primary = true;	

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

// TODO put in a CPP file shared with client.cpp
void set_socket__to_non_blocking_mode(int socketfd) {
	int flags = fcntl(socketfd, F_GETFL);
	if (flags == -1) {
		printf("FAILED getting socket flags.\n");
	}
	flags = flags | O_NONBLOCK;
	if (fcntl(socketfd, F_SETFL, flags) != 0) {
		printf("FAILED setting socket to NON-BLOCKING mode.\n");
	}
}

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
int setup_socket(int port)
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
	serv_addr.sin_port = htons(port);
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

// reads a message from a socket (receives a message through it) (BLOCKING VERSION)
void blocking_read_message(int socketfd, char* buffer)
{
	// make sure buffer is clear	
	bzero(buffer, BUFFER_SIZE);
	/* read from the socket */
	int n;
	do{
		n = read(socketfd, buffer, BUFFER_SIZE);
	} while(n < 0);
}

int receive_ack(int socket, int seqn){
	char buffer[BUFFER_SIZE];
	packet packet_received;
	blocking_read_message(socket, buffer);
	packet_received = buffer_to_packet(buffer);
	if(packet_received.seqn == seqn && packet_received.type == TYPE_ACK){
		return 0;
	} else {
		return -1;
	}
}

void send_ack_to_client(int socket, int reference_seqn)
{
	char payload[PAYLOAD_SIZE];
	char buffer[BUFFER_SIZE];
	bzero(payload, PAYLOAD_SIZE); //clear payload buffer
	packet packet_to_send = create_packet(payload, TYPE_ACK, reference_seqn);
	serialize_packet(packet_to_send, buffer);
	write_message(socket, buffer);
}

void send_error_to_client(int socket, int reference_seqn, char* error_message)
{
	char buffer[BUFFER_SIZE];
	packet packet_to_send = create_packet(error_message, TYPE_ERROR, reference_seqn);
	serialize_packet(packet_to_send, buffer);
	write_message(socket, buffer);
}

void send_notification(int socket, Row* currentRow, int seqn) {
	packet packet_to_send;
	char payload[PAYLOAD_SIZE];
	char buffer[BUFFER_SIZE];
	std::string notification;

	// send notification
	notification = currentRow->getNotification();
	strcpy(payload, notification.c_str());
	packet_to_send = create_packet(payload, TYPE_MSG, seqn);
	serialize_packet(packet_to_send, buffer);
	write_message(socket, buffer);
}

void receive_FOLLOW(packet received_packet, int socket, std::string currentUser){
	std::string newFollowedUsername(received_packet._payload); //copying char array into proper std::string type
	char payload[PAYLOAD_SIZE];

	int status = masterTable->followUser(newFollowedUsername, currentUser);
	switch(status){
		case 0:
			send_ack_to_client(socket, received_packet.seqn);
			std::cout << currentUser + " is now following " + newFollowedUsername + "." << std::endl; fflush(stdout);
			break;
		case -1:
			// user does not exist
			snprintf(payload, PAYLOAD_SIZE, "ERROR: user does not exist \n");
			send_error_to_client(socket, received_packet.seqn, payload);
			printf("ERROR: user does not exist.\n"); fflush(stdout);
			break;
		case -2:
			snprintf(payload, PAYLOAD_SIZE, "ERROR: a user cannot follow himself \n");
			send_error_to_client(socket, received_packet.seqn, payload);
			printf("ERROR: user cannot follow himself.\n"); fflush(stdout);
			break;
		case -3:
			snprintf(payload, PAYLOAD_SIZE, "ERROR: %s is already following %s\n", currentUser.c_str(), newFollowedUsername.c_str());
			send_error_to_client(socket, received_packet.seqn, payload);
			std::cout << currentUser + " is already following " + newFollowedUsername + "." << std::endl; fflush(stdout);
			break;
	}
}


std::string receive_CONNECT(packet received_packet, int socketfd, pthread_t thread_id){
	char payload[PAYLOAD_SIZE];
	Row* currentRow;
	std::string currentUser(received_packet._payload); //copying char array into proper std::string type

	// check if map already has the username in there before inserting
	masterTable->addUserIfNotExists(currentUser);
	std::cout << "User " + currentUser + " is connecting...";

	currentRow = masterTable->getRow(currentUser);
	
	if(currentRow->connectUser())
	{
		send_ack_to_client(socketfd, received_packet.seqn);
		std::cout << " connected." << std::endl;
	} else{
		snprintf(payload, PAYLOAD_SIZE, "ERROR: user already connected with 2 sessions! (Limit reached)\n");
		send_error_to_client(socketfd, received_packet.seqn, payload);
		printf("\n denied: there are already 2 active sessions!\n"); fflush(stdout);
		closeConnection(socketfd, (int)thread_id);
	}
	return currentUser;
}

void receive_DISCONNECT(std::string currentUser, int socket, pthread_t thread_id){
	Row* currentRow = masterTable->getRow(currentUser);
	currentRow->closeSession();
	std::cout << currentUser + " has disconnected. Session closed. Terminating thread " << (int)thread_id << std::endl;
	if (shutdown(socket, SHUT_RDWR) !=0 ) {
		std::cout << "Failed to gracefully shutdown a connection socket. Forcing..." << std::endl;
	}
	close(socket);
	pthread_exit(NULL);
}

int receive_ACK(packet received_packet, std::string currentUser, int seqn) {
	Row* currentRow;
	if(received_packet.seqn == seqn){
		currentRow = masterTable->getRow(currentUser);
		int activeSessions = currentRow->getActiveSessions();
		if(activeSessions == 1) {
			currentRow->popNotification();
		} else { // 2 active sessions
			// TODO consertar problema de mutex
			bool was_notification_delivered = currentRow->get_notification_delivered();
			if(was_notification_delivered) {
				// one notification was delivered for now
				currentRow->popNotification();
			} else {
				// no notification was delivered yet
				currentRow->set_notification_delivered(true);
				bool timeout_condition = false;
				std::time_t start_timestamp = std::time(nullptr);
				// wait until the other thread sends notification
				while(currentRow->get_notification_delivered() == true && !timeout_condition){
					std::time_t loop_timestamp = std::time(nullptr);
					timeout_condition = !(loop_timestamp - start_timestamp <= 3);
				}
			}
		}
		return 0;
	} else {
		return -1;
	}
}

int send_update_to_backup(int current_server_id, int seqn, int socket){
	char payload[PAYLOAD_SIZE];
	char buffer[BUFFER_SIZE];
	packet packet_to_send;
	if(current_server_id != -1) // if the backup has already connected
	{
		// TODO
	}
	return 0;
}


int send_message_to_client(std::string currentUser, int seqn, int socket){
	char payload[PAYLOAD_SIZE];
	char buffer[BUFFER_SIZE];
	std::string notification;
	packet packet_to_send;
	if(currentUser != "not-connected") // if the user has already connected
	{
		Row* currentRow = masterTable->getRow(currentUser);
		
		if(currentRow->hasNewNotification())
		{
			int activeSessions = currentRow->getActiveSessions();
			if(activeSessions == 1) {
				// consume notification and remove it from the FIFO
				send_notification(socket, currentRow, seqn);
				return 0;
			} else if(activeSessions == 2) {
				// TODO consertar aqui !!! botar um lock
				bool was_notification_delivered = currentRow->get_notification_delivered();
				if(was_notification_delivered) {
					// consume notification and remove it from the FIFO
					send_notification(socket, currentRow, seqn);
					return 0;
				} else {
					// consume notification but DO NOT remove it from the FIFO
					notification = currentRow->getNotification();
					strcpy(payload, notification.c_str());
					packet_to_send = create_packet(payload, 0, seqn);
					serialize_packet(packet_to_send, buffer);
					write_message(socket, buffer);
					return 0;
				}
			}
		}
	}
	return -1;
}

// Thread designated for the connected backup server
void * servers_socket_thread(void *arg) {
	int socket = *((int *)arg);
	int send_tries = 0;
	int size = 0;
	int seqn = 0;
	int status;
	int max_reference_seqn = -1; // TODO temos que enviar isso pras replicas backup
	char buffer[BUFFER_SIZE];
	char backup_server_message[BUFFER_SIZE];		
	int current_server_id = -1;
	packet received_packet;

	// print pthread id
	pthread_t thread_id = pthread_self();

	// setting socket to NON-BLOCKING mode
	set_socket__to_non_blocking_mode(socket);

	// Attribute a new unique incremented ID to the server

	do {
		// receive message
		size = recv(socket, backup_server_message, BUFFER_SIZE-1, 0);
		if (size > 0) {
			backup_server_message[size] = '\0';

			// parse socket buffer: get several messages, if there are more than one
			char* token_end_of_packet;
  			char* rest_packet = backup_server_message;
			while((token_end_of_packet = strtok_r(rest_packet, "\n", &rest_packet)) != NULL)
			{
				strcpy(buffer, token_end_of_packet); // put token_end_of_packet in buffer
				received_packet = buffer_to_packet(buffer);

				if(received_packet.seqn <= max_reference_seqn && received_packet.type != TYPE_ACK){
					// already received this message
					send_ack_to_client(socket, received_packet.seqn);
				} else if (received_packet.type == TYPE_ACK) {
					// TODO
				} else {
					std::cout << "Invalid message type from backup server" << std::endl;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// send update, if there is any
		status = send_update_to_backup(current_server_id, seqn, socket);
		if(status == 0){
			send_tries++;
		}
  	}while (get_termination_signal() == false && send_tries < MAX_TRIES);

	printf("Exiting socket thread: %d\n", (int)thread_id);
	if (shutdown(socket, SHUT_RDWR) !=0 ) {
		std::cout << "Failed to shutdown a connection socket." << std::endl;
	}
	close(socket);
	pthread_exit(NULL);
}

// Thread designated for the connected client
void * socket_thread(void *arg) {
	int socket = *((int *)arg);
	int send_tries = 0;
	int size = 0;
	int seqn = 0;
	int status;
	int max_reference_seqn = -1; // TODO temos que enviar isso pras replicas backup
	char client_message[BUFFER_SIZE];
	char buffer[BUFFER_SIZE];
	Row* currentRow;
	std::string currentUser = "not-connected";
	std::string notification;
	
	packet received_packet;

	// print pthread id
	pthread_t thread_id = pthread_self();

	// setting socket to NON-BLOCKING mode
	set_socket__to_non_blocking_mode(socket);

	// zero-fill the buffer
	memset(client_message, 0, sizeof client_message);

	do{
		// receive message
		size = recv(socket, client_message, BUFFER_SIZE-1, 0);
		if (size > 0) {
			client_message[size] = '\0';

			// parse socket buffer: get several messages, if there are more than one
			char* token_end_of_packet;
  			char* rest_packet = client_message;
			while((token_end_of_packet = strtok_r(rest_packet, "\n", &rest_packet)) != NULL)
			{
				strcpy(buffer, token_end_of_packet); // put token_end_of_packet in buffer
				received_packet = buffer_to_packet(buffer);

				if(received_packet.seqn <= max_reference_seqn && received_packet.type != TYPE_ACK){
					// already received this message
					send_ack_to_client(socket, received_packet.seqn);
				} else {
					switch (received_packet.type) {
						case TYPE_CONNECT:
						{
							max_reference_seqn = received_packet.seqn;
							currentUser = receive_CONNECT(received_packet, socket, thread_id);
							break;
						}
						case TYPE_FOLLOW:
						{
							max_reference_seqn = received_packet.seqn;
							receive_FOLLOW(received_packet, socket, currentUser);
							break;
						}
						case TYPE_SEND:
						{
							max_reference_seqn = received_packet.seqn;
							std::string message(received_packet._payload); //copying char array into proper std::string type
							masterTable->sendMessageToFollowers(currentUser, message);
							send_ack_to_client(socket, received_packet.seqn);
							break;
						}
						case TYPE_DISCONNECT:
						{
							max_reference_seqn = received_packet.seqn;
							receive_DISCONNECT(currentUser, socket, thread_id);
							break;
						}
						case TYPE_ACK:
						{
							status = receive_ACK(received_packet, currentUser, seqn);
							if(status == 0){
								send_tries = 0; // reset this counter
								seqn++;
							}
							break;
						}
					}
				}
			}
		}

		sleep(1);

		// send message, if there is any
		status = send_message_to_client(currentUser, seqn, socket);
		if(status == 0){
			send_tries++;
		}
  	}while (get_termination_signal() == false && send_tries < MAX_TRIES);

	printf("Exiting socket thread: %d\n", (int)thread_id);
	currentRow = masterTable->getRow(currentUser);
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
	int clients_sockfd;
	int servers_sockfd;
	int newsockfd;
	int* newsockptr;
	socklen_t clilen;
	struct sockaddr_in cli_addr;
	std::list<pthread_t> threads_list;
	pthread_t newthread;
	std::list<int*> socketptrs_list;
	communication_params communication_parameters;

	// Read arguments
	if (argc == 4 || argc == 6) {

		communication_parameters.type = argv[1]; // Server type: primary or backup?

    	communication_parameters.local_clients_listen_port = atoi(argv[2]);
    	communication_parameters.local_servers_listen_port = atoi(argv[3]);

		if ((strcmp(communication_parameters.type, "backup") == 0) && (argc == 6)) {
			// Optional args
			communication_parameters.remote_primary_server_ip_address = argv[4];
    		communication_parameters.remote_primary_server_port = atoi(argv[5]);

			is_primary = false;

		} else if ((strcmp(communication_parameters.type, "backup") == 0) && (argc != 6)) {
			std::cout << "Remote primary server IP and port were not informed." << std::endl;
			std::cout << "Usage: <type: 'backup' or 'primary'> <listen port for clients> <listen port for servers> <remote primary IP> <remote primary port>" << std::endl;
			exit(1);
		}
	}
	else {
		std::cout << "Usage: <type: 'backup' or 'primary'> <listen port for clients> <listen port for servers> <remote primary IP> <remote primary port>" << std::endl;
		exit(0);
	}

	// Install handler (assign handler to signal)
    std::signal(SIGINT, exit_hook_handler);
	std::signal(SIGTERM, exit_hook_handler);
	std::signal(SIGABRT, exit_hook_handler);

	// Initialize global MasterTable instance
	masterTable = new MasterTable;

	// load backup table if it exists
	masterTable->load_backup_table();

	// setup LISTEN sockets
    clients_sockfd = setup_socket(communication_parameters.local_clients_listen_port);
	servers_sockfd = setup_socket(communication_parameters.local_servers_listen_port);

	printf("Now listening for incoming connections... \n\n");

	clilen = sizeof(struct sockaddr_in);

	// TODO : criar uma thread e connexao (socket) para cada backup

	// loop that accepts new connections and allocates threads to deal with them
	while(1) {
		
		// If new incoming CLIENTS connection, accept. Then continue as normal.
		if ((newsockfd = accept(clients_sockfd, (struct sockaddr *) &cli_addr, &clilen)) != -1) {
			
			if((newsockptr = (int*)malloc(sizeof(int))) == NULL) {
				std::cout << "Failed to allocate enough memory for the newsockptr. (clients)" << std::endl;
				exit(-1);
			}	
			
			*newsockptr = newsockfd;
			if (pthread_create(&newthread, NULL, socket_thread, newsockptr) != 0 ) {
				printf("Failed to create clients socket thread\n");
				exit(-1);
			} else {
				threads_list.push_back(newthread);
				socketptrs_list.push_back(newsockptr);
			}
		}

		// If new incoming SERVERS connection, accept. Then continue as normal.
		if ((newsockfd = accept(servers_sockfd, (struct sockaddr *) &cli_addr, &clilen)) != -1) {
			
			if((newsockptr = (int*)malloc(sizeof(int))) == NULL) {
				std::cout << "Failed to allocate enough memory for the newsockptr. (servers)" << std::endl;
				exit(-1);
			}	
			
			*newsockptr = newsockfd;
			if (pthread_create(&newthread, NULL, servers_socket_thread, newsockptr) != 0 ) {
				printf("Failed to create servers socket thread\n");
				exit(-1);
			} else {
				threads_list.push_back(newthread);
				socketptrs_list.push_back(newsockptr);
			}
		}

		// If server is of type BACKUP, then open connection to the primary and spawn a thread
		if (is_primary == false) {
			//TODO
			// conectar ao primary, iniciar thread que trata, etc
			std::cout << "is backup" << std::endl;
			exit(1);
		}

		// Cleanup code for main server thread
		if(get_termination_signal() == true) {
			std::cout << "Server is now gracefully shutting down..." << std::endl;
			// TODO informar aos clientes q o servidor esta desligando
			std::cout << "Closing passive socket..." << std::endl;
			if (shutdown(clients_sockfd, SHUT_RDWR) !=0 ) {
				std::cout << "Failed to shutdown the clients connection socket." << std::endl;
			}
			close(clients_sockfd);
			if (shutdown(servers_sockfd, SHUT_RDWR) !=0 ) {
				std::cout << "Failed to shutdown the servers connection socket." << std::endl;
			}
			close(servers_sockfd);
			
			for (auto const& thread_id : threads_list) {
				pthread_join(thread_id, NULL);
			}

			std::cout << "Freeing allocated socket pointers memory..." << std::endl;
			for (auto const& socket_id : socketptrs_list) {
				free(socket_id);
			}

			std::cout << "Deleting instantiated objects..." << std::endl;
			masterTable->deleteRows();
			delete(masterTable);

			std::cout << "Shutdown routine successfully completed." << std::endl;
			exit(0);
		}

		sleep(1);
	}
}
