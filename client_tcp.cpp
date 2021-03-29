// codigo client_tcp.c fornecido pelo professor

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define PORT 4000
#define BUFFER_SIZE 256
#define MESSAGE_SIZE 128

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

packet create_packet(char* message, int packet_seqn, int packet_type)
{
  packet packet_to_send;
  packet_to_send._payload = message;
  packet_to_send.seqn = packet_seqn; 
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
int send_message(int sockfd, int seqn)
{
  packet packet_to_send;
  char buffer[BUFFER_SIZE];
  char message[MESSAGE_SIZE];

  printf("Please enter your message:");
  bzero(message, sizeof(message));  
  fgets(message, MESSAGE_SIZE, stdin);
  message[strlen(message)-1] = 0; // remove '\n'
  packet_to_send = create_packet(message, seqn, 3);
  serialize_packet(packet_to_send, buffer);
  int n = write(sockfd, buffer, strlen(buffer));
  if (n < 0) 
  {
    printf("ERROR writing to socket\n");
    return 0;
  }
  else
    return 1;
}

int main(int argc, char *argv[])
{
  int sockfd, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  packet packet_to_send;

  char buffer[BUFFER_SIZE];
  char message[MESSAGE_SIZE];

  if (argc < 2) {
		fprintf(stderr,"usage: %s hostname\n", argv[0]);
		exit(0);
  }
	
	server = gethostbyname(argv[1]);
	if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
    
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    printf("ERROR opening socket\n");
    
  // assign ip and port to socket
	serv_addr.sin_family = AF_INET;     
	serv_addr.sin_port = htons(PORT);    
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);     
	
  // connect client socket to server socket
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    printf("ERROR connecting\n");

	int seqn = 0;

  //// send CONNECT message
  printf("Please enter your username:");
  bzero(message, sizeof(message));  
  fgets(message, MESSAGE_SIZE, stdin);
  message[strlen(message)-1] = 0; // remove '\n'
  /* write buffer in the socket */
  packet_to_send = create_packet(message, seqn, 0);
  seqn++;
  serialize_packet(packet_to_send, buffer);
  n = write(sockfd, buffer, strlen(buffer));
  if (n < 0) 
    printf("ERROR writing to socket\n");
  /* read from the socket */
  n = read(sockfd, buffer, BUFFER_SIZE);
  if (n < 0) 
    printf("ERROR reading from socket\n");


  ///////////////////////////////////
  //// send MSG messages
  while(1){
    send_message(sockfd, seqn);

    /* read from the socket */
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) 
      printf("ERROR reading from socket\n");
    printf("Received from server: %s\n",buffer);

    seqn++;
 }
 
	close(sockfd);
  return 0;
}