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

#define PORT 4000
#define BUFFER_SIZE 256
#define PAYLOAD_SIZE 128

int seqn = 0;
sem_t send_empty, send_full, receive_empty, receive_full;

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
    const char* _payload; // Dados da mensagem
} packet;

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
		printf("ERROR: unkown packet type");
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

  printf("Please enter your message:");
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
		printf("ERROR writing to socket");
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
		printf("ERROR reading from socket");
}

// checks if there is something in the socket
// and puts it in the given buffer
// (returns -1 if there was nothing there)
int try_read_socket(int socketfd, char* buffer)
{
  int status;
  status = recv(socketfd, buffer, BUFFER_SIZE, 0) ;
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

  snprintf(payload, PAYLOAD_SIZE, "%s", profile_name); // char* to char[]
  packet_to_send = create_packet(payload, 0);
  serialize_packet(packet_to_send, buffer);
  write_message(socketfd, buffer);

  /* read ACK from the socket */
  read_message(socketfd, buffer);
  sleep(1);
  fflush(stdout);

  printf("Received message: %s\n", buffer);
  sleep(1);
  fflush(stdout);

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

  /* read ACK from the socket */
  read_message(socketfd, buffer);
  printf("DEBUG 1112 \n");
  sleep(1);
  fflush(stdout);

  printf("Received message: %s\n", buffer);
  sleep(1);
  fflush(stdout);

  printf("DEBUG 555 \n");

  sleep(3);
}



void communication_loop(int socketfd)
{
    char buffer[BUFFER_SIZE];
    char payload[PAYLOAD_SIZE];
    packet packet;

    printf("DEBUG communication_loop\n");
    // se tem mensagem pra mandar, tira da fila to_send e manda
    // sem_wait(&receive_full); // waits for the receive FIFO queue to be empty
		// se tem mensagem recebida,  bota na fila received
    bzero(buffer, BUFFER_SIZE);
    int status = recv(socketfd, buffer, BUFFER_SIZE, 0);
    if(status != -1)
    {
      printf("DEBUG status %d\n", status);
    } else {
      printf("DEBUG status %d", status);
      printf(" and buffer: %s\n", buffer);
    }


    // sem_wait(&receive_empty); // waits for the receive FIFO queue to be empty
		
    // loop antigo:
    // send_user_message(socketfd);
    // // receive ACK
    // read_message(socketfd, buffer_received);
    // printf("Received from server: %s\n", buffer_received);

    sleep(1);
}

// function for the thread that deals with communication
void * communication_thread(void *arg) {
  // TODO pegar ponteiro pra
  // - fila de mensagens recebidas
  // - fila de mensagens a enviar 
	communication_params params = *((communication_params *)arg);
  int socketfd;

  // print pthread id
	pthread_t thread_id = pthread_self();
	printf("Started thread %d\n", (int)thread_id);

  socketfd = setup_socket(params);
  send_connect_message(socketfd, params.profile_name);

  // Test a FOLLOW packet
  char name[99] = "andre";
  char* follow_name = &name[0];
  send_follow_message(socketfd, follow_name);

	while(1){
    communication_loop(socketfd);
  }

	printf("Exiting socket thread: %d\n", (int)thread_id);
	close(socketfd);
	pthread_exit(NULL);
}


// function for the thread that deals with the user interface
void * interface_thread(void *arg) {
	interface_params params = *((interface_params *)arg);

  // print pthread id
	pthread_t thread_id = pthread_self();
	printf("Started thread %d\n", (int)thread_id);

	// while(1){
  //   // TODO permitir usuario dizer MSG ou FOLLOW
  // }
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
  sem_init(&send_empty, 1, 1);
  sem_init(&send_full, 1, 0);
  sem_init(&receive_empty, 1, 1);
  sem_init(&receive_full, 1, 0);

  // create thread for communication with the server
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