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

#define PORT 4000
#define BUFFER_SIZE 256
#define PAYLOAD_SIZE 128

int seqn = 0;

typedef struct __communication_params{
  char * profile_name;
  char * server_ip_address;
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
int send_user_message(int sockfd)
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
  int n = write(sockfd, buffer, strlen(buffer));
  if (n < 0) 
  {
    printf("ERROR writing to socket\n");
    return 0;
  }
  else
    return 1;
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

// writes a message from a socket (receives a message through it)
void read_message(int newsockfd, char* buffer)
{
	// make sure buffer is clear	
  bzero(buffer, BUFFER_SIZE);
	/* read from the socket */
  int n;
	n = read(newsockfd, buffer, BUFFER_SIZE);
	if (n < 0) 
		printf("ERROR reading from socket");
}

// function for the thread that deals with communication
void * communication_thread(void *arg) {
  // TODO pegar ponteiro pra
  // - fila de mensagens recebidas
  // - fila de mensagens a enviar 
	communication_params params = *((communication_params *)arg);
  int sockfd, n;

  struct sockaddr_in serv_addr;
  struct hostent *server;
  packet packet_to_send;

  char * profile_name;
  char * server_ip_address;
  int port;

  char buffer_to_send[BUFFER_SIZE];
  char buffer_received[BUFFER_SIZE];
  char payload[PAYLOAD_SIZE];

  // print pthread id
	pthread_t thread_id = pthread_self();
	printf("Started thread %d\n", (int)thread_id);

  server = gethostbyname(params.server_ip_address);
	if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
    
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    printf("ERROR opening socket\n");
    
  // assign ip and port to socket
	serv_addr.sin_family = AF_INET;     
	serv_addr.sin_port = htons(params.port);    
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);     
	
  // connect client socket to server socket
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    printf("ERROR connecting\n");


  //// send CONNECT message
  snprintf(payload, PAYLOAD_SIZE, "%s", params.profile_name);
  packet_to_send = create_packet(payload, 0);
  serialize_packet(packet_to_send, buffer_to_send);
  write_message(sockfd, buffer_to_send);
  /* read ACK from the socket */
  read_message(sockfd, buffer_received);
  printf("Received message: %s\n", buffer_received);

	while(1){
    // TODO check fila de mensagens a enviar
    // TODO get mensagem a enviar
    send_user_message(sockfd);

		// receive ACK
    read_message(sockfd, buffer_received);
    printf("Received from server: %s\n", buffer_received);
  }

	printf("Exiting socket thread: %d\n", (int)thread_id);
	close(sockfd);
	pthread_exit(NULL);
}


// function for the thread that deals with the user interface
void * interface_thread(void *arg) {
  // TODO pegar ponteiro pra
  // - fila de mensagens recebidas
  // - fila de mensagens a enviar 
	interface_params params = *((interface_params *)arg);

  // print pthread id
	pthread_t thread_id = pthread_self();
	printf("Started thread %d\n", (int)thread_id);
  
  pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
  int sockfd, n;
  int size = 0;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  packet packet_to_send;
  communication_params communication_parameters;
  interface_params interface_parameters;

  char * profile_name;
  char * server_ip_address;
  int port;
  
  char buffer[BUFFER_SIZE];
  char message[PAYLOAD_SIZE];

  pthread_t communication_tid, interface_tid;

  if (argc != 4) {
		fprintf(stderr,"usage: %s <profile_name> <server_ip_address> <port>\n", argv[0]);
		exit(0);
  } else {
    communication_parameters.profile_name = argv[1];
    communication_parameters.server_ip_address = argv[2];
    communication_parameters.port = atoi(argv[3]);
  }
	

  // TODO criar uma thread pra receber notificacoes
  if (pthread_create(&communication_tid, NULL, communication_thread, &communication_parameters) != 0 ) {
    printf("Failed to create thread\n");
    exit(-1);
  }
  // TODO criar uma thread pra interface
  if (pthread_create(&interface_tid, NULL, interface_thread, &interface_parameters) != 0 ) {
    printf("Failed to create thread\n");
    exit(-1);
  }

  pthread_join(communication_tid, NULL);
  pthread_join(interface_tid, NULL);

  return 0;
}