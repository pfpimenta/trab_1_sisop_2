#pragma once

#ifndef PACKET_H
#define PACKET_H
#include "../include/packet.hpp"
#endif

#ifndef MASTER_TABLE_H
#define PACKET_H
#include "../include/packet.hpp"
#endif
#include "../include/MasterTable.hpp"

MasterTable* masterTable;

pthread_mutex_t termination_signal_mutex = PTHREAD_MUTEX_INITIALIZER;
// int reader_counter = 0;

#define PORT 4000
#define MAX_THREADS 30 // maximum number of threads allowed
#define BUFFER_SIZE 256
#define PAYLOAD_SIZE 128

int seqn = 0;

// Global termination flag, set by the signal handler.
bool termination_signal = false;

bool get_termination_signal();

void set_termination_signal();

void closeConnection(int socket, int thread_id);

// writes a message in a socket (sends a message through it)
void write_message(int newsockfd, char* message);

// opens a socket, binds it, and starts listening for incoming connections
int setup_socket();

int accept_connection(int sockfd);

// Thread designated for the connected client
void * socket_thread(void *arg);

void exit_hook_handler(int signal);

int main(int argc, char *argv[]);