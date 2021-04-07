// codigo client_tcp.c fornecido pelo professor

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

#define PORT 4000
#define BUFFER_SIZE 256
#define PAYLOAD_SIZE 128

int seqn = 0;
sem_t is_empty, is_full;

typedef struct __communication_params{
  char* profile_name;
  char* server_ip_address;
  int port;
} communication_params;

typedef struct __interface_params{
  int messages_to_send; // TODO mudar tipo
  int received_notifications; // TODO mudar tipo
} interface_params;

// TODO : put in another file
typedef struct __packet{
    uint16_t type; // Tipo do pacote:
        // 0 - CONNECT (username_to_login, seqn)
        // 1 - FOLLOW (username_to_follow, seqn)
        // 2 - SEND (message_to_send, seqn)
        // 3 - MSG (username, message_sent, seqn)
        // 4 - ACK (seqn)
        // 5 - ERROR (seqn)
    uint16_t seqn; // Número de sequência
    uint16_t length; // Comprimento do payload
    char* _payload; // Dados da mensagem
} packet;


// TODO put somewhere else
// TODO Mutex
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

  //seqn
  token = strtok(buffer, delimiter);
  packet.seqn = atoi(token);

  //payload_length
  token = strtok(NULL, delimiter);
  packet.length = atoi(token);

  //packet_type
  token = strtok(NULL, delimiter);
  packet.type = atoi(token);

  //payload (get whatever else is in there)
  bzero(payload, PAYLOAD_SIZE); //clear payload buffer
  token = strtok(NULL, "");
  strncpy(payload, token, packet.length+1);
  packet._payload = (char*) malloc(packet.length * sizeof(char));
  memcpy(packet._payload, payload, packet.length * sizeof(char));

  return packet;
}

// serializes the packet and puts it in the buffer
void serialize_packet(packet packet_to_send, char* buffer)
{
  bzero(buffer, sizeof(buffer));
  snprintf(buffer, BUFFER_SIZE, "%u,%u,%u,%s",
          packet_to_send.seqn, packet_to_send.length, packet_to_send.type, packet_to_send._payload);
}

// asks the user to write a message and then sends it to the server
int send_user_message(int socketfd)
{
  packet packet_to_send;
  char buffer[BUFFER_SIZE];
  char message[PAYLOAD_SIZE];

  printf("\nPlease enter your message:");
  bzero(message, sizeof(message));  
  fgets(message, PAYLOAD_SIZE, stdin);
  message[strlen(message)-1] = 0; // remove '\n'
  packet_to_send = create_packet(message, 3);
  serialize_packet(packet_to_send, buffer);
  printf("Sending to server: %s\n",buffer);
  int n = write(socketfd, buffer, strlen(buffer));
  if (n < 0) 
  {
    printf("ERROR writing to socket\n");
    return 0;
  }
  else
    return 1;
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

// checks if there is something in the socket
// and puts it in the given buffer
// (returns -1 if there was nothing there)
int try_read_socket(int socketfd, char* buffer)
{
  int status;
  status = recv(socketfd, buffer, BUFFER_SIZE-1, 0) ;
  return status;
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

  // /* read ACK from the socket */
  // read_message(socketfd, buffer);
  // sleep(1);
  // fflush(stdout);

  // printf("Received message: %s\n", buffer);
  // sleep(1);
  // fflush(stdout);

  sleep(3);
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

  // /* read ACK from the socket */
  // read_message(socketfd, buffer);
  // sleep(1);
  // fflush(stdout);

  // printf("Received message: %s\n", buffer);
  // sleep(1);
  // fflush(stdout);
}

void print_commands()
{
  printf("\nCommands:");
  printf("\n - SEND: to send a message to all followers.\n    usage: \'SEND message\'");
  printf("\n - FOLLOW: to follow another user.\n    usage: \'FOLLOW username\'");
  printf("\n - NOTIFICATIONS: to show received notifications.\n    usage: \'NOTIFICATIONS\'");
  printf("\n\n");
  fflush(stdout);
}

void communication_loop(int socketfd)
{
    char buffer[BUFFER_SIZE];
    char payload[PAYLOAD_SIZE];
    packet packet;

    // printf("DEBUG communication_loop\n");
		// se tem mensagem recebida, prints it to the client
    bzero(buffer, BUFFER_SIZE);
    int size = recv(socketfd, buffer, BUFFER_SIZE-1, 0);
    if(size > 0) //(size != -1)
    {
      // printf("\nDEBUG Message received: '%s' - size: %d\n", buffer, size);
      //put the message in a packet
      packet = buffer_to_packet(buffer);
      print_packet(packet); // DEBUG
      // put the packet in the received FIFO
      packets_received_fifo.push_back(packet);
    } else {
      // nothing to read from the socket
    }

    // send packet to server, if there is any
    if(!packets_to_send_fifo.empty())
		{
      // get the packet
      packet = packets_to_send_fifo.front();
			packets_to_send_fifo.pop_front();

      // serialize and send the packet
      serialize_packet(packet, buffer);
      write_message(socketfd, buffer);
    }
}

// function for the thread that
// - receives notifications from the server, then prints them
// - gets packets to_send from the FIFO and send it to the server
void * communication_thread(void *arg) {
	communication_params params = *((communication_params *)arg);
  int socketfd;

  // print pthread id
	pthread_t thread_id = pthread_self();
	printf("Started thread %d\n", (int)thread_id);

  socketfd = setup_socket(params);
  send_connect_message(socketfd, params.profile_name);

  // Test a FOLLOW packet
  char name[99] = "usuario_teste";
  char* follow_name = &name[0];
  send_follow_message(socketfd, follow_name);

	while(1){
    communication_loop(socketfd);
  }

	printf("Exiting socket thread: %d\n", (int)thread_id);
	close(socketfd);
	pthread_exit(NULL);
}


// function for the thread that gets user input
// and puts it in the to_send FIFO queue
void * interface_thread(void *arg) {
	interface_params params = *((interface_params *)arg);
  char payload[PAYLOAD_SIZE];
  char user_input[PAYLOAD_SIZE];
  char string_to_parse[PAYLOAD_SIZE];
  packet packet_to_send;
  packet packet_received;

  char* parse_ptr;
  char* tail_ptr;
	char delim[3] = " ";
  int num_notifications;

  // print pthread id
	pthread_t thread_id = pthread_self();
	printf("Started thread %d\n", (int)thread_id);

  print_commands();

  while(1){
    printf("Please enter your message:");
    bzero(user_input, PAYLOAD_SIZE);  
    fgets(user_input, PAYLOAD_SIZE, stdin);

    // parse user input
    strcpy(string_to_parse, user_input);
    printf("DEBUG string_to_parse: %s", string_to_parse);
	  parse_ptr = strtok(string_to_parse, delim);
    if (strcmp(parse_ptr, "FOLLOW") == 0) 
    {
      user_input[strlen(user_input)-1] = 0; // remove '\n'
      tail_ptr = &user_input[7]; // get username

      printf("DEBUG FOLLOW - payload: '%s'\n", tail_ptr);

      // put the message in a packet
      bzero(payload, sizeof(payload));
      snprintf(payload, PAYLOAD_SIZE, "%s", tail_ptr);
      packet_to_send = create_packet(payload, 1);
      
      // put the packet in the FIFO queue
      packets_to_send_fifo.push_back(packet_to_send);
    } else if (strcmp(parse_ptr, "SEND") == 0) {
      user_input[strlen(user_input)-1] = 0; // remove '\n'
      tail_ptr = &user_input[5]; // get message

      printf("DEBUG SEND - payload: '%s'\n", tail_ptr);

      // put the message in a packet
      bzero(payload, sizeof(payload));
      snprintf(payload, PAYLOAD_SIZE, "%s", tail_ptr);
      packet_to_send = create_packet(payload, 2);
      
      // put the packet in the FIFO queue
      packets_to_send_fifo.push_back(packet_to_send);

    } else if (strcmp(string_to_parse, "NOTIFICATIONS\n") == 0) {  
      num_notifications = packets_received_fifo.size();
      printf("\nPrinting %d new notifications...\n", num_notifications);
      // prints received packets, if any
      while(!packets_received_fifo.empty())
      {
        // get the packet
        packet_received = packets_received_fifo.front();
        packets_received_fifo.pop_front();

        // print notification
        printf("Notification: %s\n", packet_received._payload);
        
        // free malloc
        free(packet_received._payload);
      }

    } else {
      printf("ERROR: did not recognize command: %s\n", parse_ptr);
    }
  }
  sleep(1);
  
	printf("Exiting threads %d\n", (int)thread_id);
  pthread_exit(NULL);
}

int main(int argc, char*argv[])
{
  communication_params communication_parameters;
  interface_params interface_parameters;
  pthread_t communication_tid, interface_tid;

  // get arguments
  if (argc != 4) {
		fprintf(stderr,"usage: %s <profile_name> <server_ip_address> <port>\n", argv[0]);
		exit(0);
  } else {
    communication_parameters.profile_name = argv[1];
    communication_parameters.server_ip_address = argv[2];
    communication_parameters.port = atoi(argv[3]);
  }

  // initialize semaphore variables
  sem_init(&is_empty, 1, 1);
  sem_init(&is_full, 1, 1);
  
  // create thread for receiving server packets
  if (pthread_create(&communication_tid, NULL, communication_thread, &communication_parameters) != 0 ) {
    printf("Failed to create thread\n");
    exit(-1);
  }
  // create thread for the user interface
  if (pthread_create(&interface_tid, NULL, interface_thread, &interface_parameters) != 0 ) {
    printf("Failed to create thread\n");
    exit(-1);
  }

  // waits for both threads to end before exiting the program
  pthread_join(communication_tid, NULL);
  pthread_join(interface_tid, NULL);

  return 0;
}