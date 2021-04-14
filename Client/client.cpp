#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

#define BUFFER_SIZE 256
#define PAYLOAD_SIZE 128

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

pthread_mutex_t termination_signal_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t packets_to_send_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t packets_received_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global termination flag, set by the signal handler.
bool termination_signal = false;

bool get_termination_signal(){
  pthread_mutex_lock(&termination_signal_mutex);
  bool signal = termination_signal;
  pthread_mutex_unlock(&termination_signal_mutex);
  return signal;
}

void set_termination_signal(bool new_signal_value){
  pthread_mutex_lock(&termination_signal_mutex);
  termination_signal = new_signal_value;
  pthread_mutex_unlock(&termination_signal_mutex);
}

int seqn = 0;

typedef struct __communication_params{
  char* profile_name;
  char* server_ip_address;
  int port;
} communication_params;

typedef struct __packet{
    uint16_t type; // Tipo do pacote:
        // 0 - CONNECT (username_to_login, seqn)
        // 1 - FOLLOW (username_to_follow, seqn)
        // 2 - SEND (message_to_send, seqn)
        // 3 - MSG (username, message_sent, seqn)
        // 4 - ACK (seqn)
        // 5 - ERROR (seqn)
        // 6 - DISCONNECT (seqn)
    uint16_t seqn; // Número de sequência
    uint16_t length; // Comprimento do payload
    char* _payload; // Dados da mensagem
} packet;


std::list<packet> packets_to_send_fifo;
std::list<packet> packets_received_fifo;


const char* get_packet_type_string(int packet_type)
{
  switch (packet_type)
  {
  case 0:
    return "CONNECT";
    break;
  case 1:
    return "FOLLOW";
    break;
  case 2:
    return "SEND";
    break;
  case 3:
    return "MSG";
    break;
  case 4:
    return "ACK";
    break;
  case 5:
    return "ERROR";
    break;
  case 6:
    return "DISCONNECT";
    break;
  default:
    printf("ERROR: invalid packet type\n");
    return NULL;
    break;
  }
}

int get_packet_type(char* packet_type_string)
{

  if (strcmp(packet_type_string, "CONNECT") == 0) 
  {
    return 0;
  } 
  else if (strcmp(packet_type_string, "FOLLOW") == 0)
  {
    return 1;
  }
  else if (strcmp(packet_type_string, "SEND") == 0)
  {
    return 2;
  }
  else if (strcmp(packet_type_string, "MSG") == 0)
  {
    return 3;
  }
  else if (strcmp(packet_type_string, "ACK") == 0)
  {
    return 4;
  }
  else if (strcmp(packet_type_string, "ERROR") == 0)
  {
    return 5;
  }
  else if (strcmp(packet_type_string, "DISCONNECT") == 0)
  {
    return 6;
  }
  else
  {
		printf("ERROR: unkown packet type\n");
    exit(0);
  }
}

packet create_packet(char* message, int packet_type)
{
  packet packet_to_send;
  packet_to_send._payload = message;
  packet_to_send.seqn = seqn;
  seqn++;
  packet_to_send.length = strlen(packet_to_send._payload);
  packet_to_send.type = packet_type;
  return packet_to_send;
}

void print_packet(packet packet)
{
  printf("Reference seqn: %i \n", packet.seqn);
  printf("Payload length: %i \n", packet.length);
  printf("Packet type: %i \n", packet.type);
  printf("Payload: %s \n", packet._payload);
  fflush(stdout);
}

packet buffer_to_packet(char* buffer){
  packet packet;
  char payload[PAYLOAD_SIZE];

  int buffer_size = strlen(buffer);
  buffer[buffer_size] = '\0';

  char* token;
  const char delimiter[2] = ",";
  char* rest = buffer;

  //seqn
  token = strtok_r(rest, delimiter, &rest);
  packet.seqn = atoi(token);

  //payload_length
  token = strtok_r(rest, delimiter, &rest);
  packet.length = atoi(token);

  //packet_type
  token = strtok_r(rest, delimiter, &rest);
  packet.type = atoi(token);

  //payload (get whatever else is in there)
  bzero(payload, PAYLOAD_SIZE); //clear payload buffer
  token = strtok_r(rest, "", &rest);
  strncpy(payload, token, packet.length);
  payload[packet.length] = '\0';
  packet._payload = (char*) malloc((packet.length) * sizeof(char) + 1);
  memcpy(packet._payload, payload, (packet.length) * sizeof(char) + 1);

  return packet;
}

// serializes the packet and puts it in the buffer
void serialize_packet(packet packet_to_send, char* buffer)
{
  memset(buffer, 0, BUFFER_SIZE * sizeof(char));
  snprintf(buffer, BUFFER_SIZE, "%u,%u,%u,%s\n",
          packet_to_send.seqn, packet_to_send.length, packet_to_send.type, packet_to_send._payload);
}

// writes a message in a socket (sends a message through it)
void write_message(int socketfd, char* message)
{
	/* write in the socket */
    int n;
	n = write(socketfd, message, strlen(message));
	if (n < 0) 
		printf("ERROR writing to socket\n");
}

// reads a message from a socket (receives a message through it)
void read_message(int socketfd, char* buffer)
{
	// make sure buffer is clear	
  bzero(buffer, BUFFER_SIZE);
	/* read from the socket */
  int n;
	n = read(socketfd, buffer, BUFFER_SIZE);
	if (n < 0) 
		printf("ERROR reading from socket\n");
}

int setup_socket(communication_params params)
{
  int socketfd;
  struct hostent *server;
  struct sockaddr_in serv_addr;

  server = gethostbyname(params.server_ip_address);
	if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
    
  if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    printf("ERROR opening socket\n");
    
  // assign ip and port to socket
	serv_addr.sin_family = AF_INET;     
	serv_addr.sin_port = htons(params.port);    
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);     
	
  // connect client socket to server socket
	if (connect(socketfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    printf("ERROR connecting\n");

  // change socket to non-blocking mode
  fcntl(socketfd, F_SETFL, fcntl(socketfd, F_GETFL, 0) | O_NONBLOCK);
  
  return socketfd;
}

void send_connect_message(int socketfd, char* profile_name)
{
  char buffer[BUFFER_SIZE];
  char payload[PAYLOAD_SIZE];
  packet packet_to_send;

  // send CONNECT message
  snprintf(payload, PAYLOAD_SIZE, "%s", profile_name); // char* to char[]
  packet_to_send = create_packet(payload, 0);
  serialize_packet(packet_to_send, buffer);
  write_message(socketfd, buffer);

  sleep(1);
}

void send_follow_message(int socketfd, char* profile_name)
{
  char buffer[BUFFER_SIZE];
  char payload[PAYLOAD_SIZE];
  packet packet_to_send;

  snprintf(payload, PAYLOAD_SIZE, "%s", profile_name); // char* to char[]
  packet_to_send = create_packet(payload, 1);
  serialize_packet(packet_to_send, buffer);
  write_message(socketfd, buffer);
}

void print_commands()
{
  printf("\nCommands:");
  printf("\n - SEND: to send a message to all followers.\n    usage: \'SEND message\'");
  printf("\n - FOLLOW: to follow another user.\n    usage: \'FOLLOW username\'");
  printf("\n\n");
  fflush(stdout);
}

void communication_loop(int socketfd)
{
    char buffer[BUFFER_SIZE];
    char payload[PAYLOAD_SIZE];
    packet packet;

		do {
      // se tem mensagem recebida, prints it to the client
      memset(buffer, 0, sizeof buffer);
      int size = recv(socketfd, buffer, BUFFER_SIZE-1, 0);
      if(size > 0) //(size != -1)
      {
        //put the message in a packet
        packet = buffer_to_packet(buffer);
        // put the packet in the received FIFO
        pthread_mutex_lock(&packets_received_mutex);
        packets_received_fifo.push_back(packet);
        pthread_mutex_unlock(&packets_received_mutex);
      }

      // send packet to server, if there is any
      pthread_mutex_lock(&packets_to_send_mutex);
      if(!packets_to_send_fifo.empty())
      {
        // get the packet
        packet = packets_to_send_fifo.front();
        packets_to_send_fifo.pop_front();

        // serialize and send the packet
        serialize_packet(packet, buffer);

        write_message(socketfd, buffer);
      }
      pthread_mutex_unlock(&packets_to_send_mutex);
    } while (get_termination_signal() == false);

    printf("\nClosing server connection...\nTerminating client....\n");
    // Send a DISCONNECT packet
    snprintf(payload, PAYLOAD_SIZE, " ");
    packet = create_packet(payload, 6);
    serialize_packet(packet, buffer);
    write_message(socketfd, buffer);
    sleep(1);
}

// function for the thread that
// - receives notifications from the server, then prints them
// - gets packets to_send from the FIFO and send it to the server
void * communication_thread(void *arg) {
	communication_params params = *((communication_params *)arg);
  int socketfd;

  // print pthread id
	pthread_t thread_id = pthread_self();

  // setup socket and send CONNECT message
  socketfd = setup_socket(params);
  send_connect_message(socketfd, params.profile_name);

  communication_loop(socketfd);

	printf("Exiting socket thread: %d\n", (int)thread_id);
  if (shutdown(socketfd, SHUT_RDWR) !=0 ) {
    std::cout << "Failed to gracefully shutdown a connection socket. Forcing..." << std::endl;
  }
	close(socketfd);
	pthread_exit(NULL);
}


// function for the thread that gets user input
// and puts it in the to_send FIFO queue
void * interface_thread(void *arg) {
  char payload[PAYLOAD_SIZE];
  char user_input[PAYLOAD_SIZE];
  char string_to_parse[PAYLOAD_SIZE];
  packet packet_to_send;
  packet packet_received;

  char* parse_ptr;
  char* rest;
  char* tail_ptr;
	char delim[3] = " ";
  int num_notifications;

  // print pthread id
	pthread_t thread_id = pthread_self();

  print_commands();

  printf("> "); fflush(stdout);

  struct pollfd stdin_poll = { .fd = STDIN_FILENO
                           , .events = POLLIN | POLLRDBAND | POLLRDNORM | POLLPRI };

  while(get_termination_signal() == false){
    bzero(user_input, PAYLOAD_SIZE);  

    if (poll(&stdin_poll, 1, 0) == 1) {
      /* Data waiting on stdin. Process it. */
      
      if(fgets(user_input, PAYLOAD_SIZE, stdin) == NULL) {

        // ctrl+D detected
        set_termination_signal(true);
      
      } else {
        // parse user input
        rest = string_to_parse;
        strcpy(string_to_parse, user_input);
        parse_ptr = strtok_r(string_to_parse, delim, &rest);
        if (strcmp(parse_ptr, "FOLLOW") == 0) 
        {
          user_input[strlen(user_input)-1] = 0; // remove '\n'
          tail_ptr = &user_input[7]; // get username

          // put the message in a packet
          bzero(payload, sizeof(payload));
          snprintf(payload, PAYLOAD_SIZE, "%s", tail_ptr);
          packet_to_send = create_packet(payload, 1);
          
          // put the packet in the FIFO queue
          pthread_mutex_lock(&packets_to_send_mutex);
          packets_to_send_fifo.push_back(packet_to_send);
          pthread_mutex_unlock(&packets_to_send_mutex);
        } else if (strcmp(parse_ptr, "SEND") == 0) {
          user_input[strlen(user_input)-1] = 0; // remove '\n'
          tail_ptr = &user_input[5]; // get message

          // put the message in a packet
          bzero(payload, sizeof(payload));
          snprintf(payload, PAYLOAD_SIZE, "%s", tail_ptr);
          packet_to_send = create_packet(payload, 2);
          
          // put the packet in the FIFO queue
          pthread_mutex_lock(&packets_to_send_mutex);
          packets_to_send_fifo.push_back(packet_to_send);
          pthread_mutex_unlock(&packets_to_send_mutex);

        } else {
          printf("ERROR: did not recognize command: %s\n", parse_ptr);
        }

        printf("> "); fflush(stdout);

      }
    }
    
    /* Do other processing: check for incoming packets */
    pthread_mutex_lock(&packets_received_mutex);
    while(!packets_received_fifo.empty())
    {
      // get the packet
      packet_received = packets_received_fifo.front();
      packets_received_fifo.pop_front();

      // print notification
      printf("Notification: " ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET "\n", packet_received._payload);

      printf("\n> "); fflush(stdout);
      
    }
    pthread_mutex_unlock(&packets_received_mutex);

  }

	printf("Exiting threads %d\n", (int)thread_id);
  pthread_exit(NULL);
}

void exit_hook_handler(int signal) {
	std::cout<< std::endl << "Signal received: code is " << signal << std::endl;
  set_termination_signal(true);
}

int main(int argc, char*argv[])
{
  communication_params communication_parameters;
  pthread_t communication_tid, interface_tid;

  // Install handler (assign handler to signal)
  std::signal(SIGINT, exit_hook_handler);
  std::signal(SIGTERM, exit_hook_handler);
  std::signal(SIGABRT, exit_hook_handler);

  // get arguments
  if (argc != 4) {
		fprintf(stderr,"usage: %s <profile_name> <server_ip_address> <port>\n", argv[0]);
		exit(0);
  } else {
    communication_parameters.profile_name = argv[1];
    communication_parameters.server_ip_address = argv[2];
    communication_parameters.port = atoi(argv[3]);
  }
  
  // create thread for receiving server packets
  if (pthread_create(&communication_tid, NULL, communication_thread, &communication_parameters) != 0 ) {
    printf("Failed to create thread\n");
    exit(-1);
  }
  // create thread for the user interface
  if (pthread_create(&interface_tid, NULL, interface_thread, NULL) != 0 ) {
    printf("Failed to create thread\n");
    exit(-1);
  }

  // wait for communication thread to finish before exiting the program
  pthread_join(communication_tid, NULL);

  // terminate interface thread
  pthread_cancel(interface_tid);

  exit(0);
}