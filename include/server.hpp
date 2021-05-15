#pragma once
#include <algorithm>
#include <arpa/inet.h>
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
#include <netdb.h> 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#ifndef SESSION_H
#define SESSION_H
#include "../include/session.hpp"
#endif

#ifndef PACKET_H
#define PACKET_H
#include "../include/packet.hpp"
#endif

#ifndef ROW_H
#define ROW_H
#include "../include/Row.hpp"
#endif

#ifndef MASTER_TABLE_H
#define MASTER_TABLE_H
#include "../include/MasterTable.hpp"
#endif

#define PORT 4000
#define MAX_THREADS 30 // maximum number of threads allowed
#define MAX_TRIES 3 // maximum number of tries to send a notification to a client
#define UNDEFINED_ID -1

typedef struct __communication_params{
  char* profile_name;
  char* server_ip_address;
  int port;
  int local_listen_port;
} communication_params;

typedef struct __server_params{
  char* type; // BACKUP ou PRIMARY
  int local_clients_listen_port;
  int local_servers_listen_port;
  char* remote_primary_server_ip_address; // only if type= "backup"
  int remote_primary_server_port; // only if type= "backup"
} server_params;

typedef struct __client {
  char* ip;
  int port;
  int socketfd;
} client_struct;

typedef struct __server {
  int server_id;
  char* ip;
  int port; // porta de listen para outros backups
} server_struct;

void print_server_struct(server_struct server_infos);

int get_next_session_id();

bool get_termination_signal();

void set_termination_signal();

// TODO put in a CPP file shared with client.cpp
void set_socket_to_non_blocking_mode(int socketfd);

void closeConnection(int socket, int thread_id);

// writes a message in a socket (sends a message through it)
int write_message(int newsockfd, char* message);

void terminate_thread_and_socket(int socketfd);

// socket that connects to an existing socket
int setup_socket(communication_params params);

// opens a socket, binds it, and starts listening for incoming connections
int setup_listen_socket(int port);

int accept_connection(int sockfd);

// reads a message from a socket (receives a message through it) (BLOCKING VERSION)
void blocking_read_message(int socketfd, char* buffer);

int receive_ack(int socket, int seqn);

void send_ACK(int socket, int reference_seqn);

void send_error_to_client(int socket, int reference_seqn, char* error_message);

void send_notification(int socket, Row* currentRow, int seqn);

void receive_FOLLOW(packet received_packet, int socket, std::string currentUser);

std::string receive_CONNECT(packet received_packet, int socketfd, pthread_t thread_id, int seqn, int session_id);

void receive_DISCONNECT(std::string currentUser, int socket, pthread_t thread_id, int session_id);

int receive_SET_ID(int socket);

// treats ACK received from client
int receive_ACK(packet received_packet, std::string currentUser, int seqn);

server_struct receive_CONNECT_SERVER(int socketfd);

int send_UPDATE_BACKUP(int current_server_id, int seqn, int socket);

int send_message_to_client(std::string currentUser, int seqn, int socket);

// send CONNECT_SERVER and wait for ACK. returns 0 if success
int send_CONNECT_SERVER(int socketfd, int seqn, int port);

void send_ping_to_primary(int socketfd, int seqn);

// send SET_ID and wait for ACK. returns 0 if success
int send_SET_ID(int socketfd, int seqn, int backup_id);

// Thread designated to communicate with the primary server (thread roda no backup)
void * primary_communication_thread(void *arg);

// Thread designated for the connected backup server (thread roda no primario)
void * servers_socket_thread(void *arg);

// Thread designated for the connected client
void * socket_thread(void *arg);

void exit_hook_handler(int signal);

int main(int argc, char *argv[]);
