#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <list>
#include <csignal>
#include <iostream>
#include <poll.h>

#include "../include/packet.hpp"

#define MAX_TRIES 3

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef struct __communication_params{
  char* profile_name;
  char* server_ip_address;
  int port;
  int local_listen_port;
} communication_params;

bool get_termination_signal();

void set_termination_signal(bool new_signal_value);

// writes a message in a socket (sends a message through it)
int write_message(int socketfd, char* message);

// reads a message from a socket (receives a message through it)
int read_message(int socketfd, char* buffer);

// reads a message from a socket (receives a message through it) (BLOCKING VERSION)
void blocking_read_message(int socketfd, char* buffer);

int setup_listen_socket(int port);

int setup_socket(communication_params params);

int send_connect_message(int socketfd, char* profile_name, int local_listen_port);

int send_message(int socketfd, char* buffer);

void send_ACK(int socketfd, int reference_seqn);

void print_commands();

void terminate_thread_and_socket(int socketfd);

void communication_loop(int socketfd);

// function for the thread that
// - receives notifications from the server, then prints them
// - gets packets to_send from the FIFO and send it to the server
void * communication_thread(void *arg);

// function for the thread that receives a server change message
// from the new primary server
void* server_change_thread(void *arg);

// function for the thread that keeps listening for
// substitutions of the primary server
void * listen_thread(void *arg);

// function for the thread that gets user input
// and puts it in the to_send FIFO queue
void * interface_thread(void *arg);

void exit_hook_handler(int signal);

int main(int argc, char*argv[]);