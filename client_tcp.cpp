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

// TODO : put in another file
typedef struct __packet{
    uint16_t type; // Tipo do pacote (p.ex. DATA | CMD)
    uint16_t seqn; // Número de sequência
    uint16_t length; // Comprimento do payload
    //uint16_t timestamp; // Timestamp do dado // provavelmente nao precisamos disso
    const char* _payload; // Dados da mensagem
        // CONNECT username_to_login
        // FOLLOW username_to_follow
        // SEND message_to_send
        // MSG username timestamp message_sent
        // ACK seqn
        // ERROR seqn
} packet;


int main(int argc, char *argv[])
{
  int sockfd, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  char buffer[256];
  char message[256];

  if (argc < 2) {
		fprintf(stderr,"usage %s hostname\n", argv[0]);
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

  // put the message in a packet
  packet packet_to_send;
  size_t payload_length = snprintf(message, 256, "mensagem DEBUGadora dos packet");
  packet_to_send._payload = message;
  packet_to_send.seqn = 0; 
  packet_to_send.length = strlen(packet_to_send._payload);
  packet_to_send.type = 0; // TODO botar o que aqui?
  
  // put packet in a buffer
  // TODO
  bzero(buffer, sizeof(buffer));


	int seqn = 0;

  //// send CONNECT message
  /* write buffer in the socket */
  bzero(buffer, sizeof(buffer));
  size_t buffer_length = snprintf(buffer, 256, "CONNECT,username");
  n = write(sockfd, buffer, strlen(buffer));
  if (n < 0) 
    printf("ERROR writing to socket\n");
  /* read from the socket */
  n = read(sockfd, buffer, 256);
  if (n < 0) 
    printf("ERROR reading from socket\n");


  //// send MSG message
  while(1){
    /* write buffer in the socket */
		bzero(buffer, sizeof(buffer));
		snprintf(buffer, 14, "MSG, %08d", seqn);
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) 
      printf("ERROR writing to socket\n");
    
    /* read from the socket */
    n = read(sockfd, buffer, 256);
    if (n < 0) 
      printf("ERROR reading from socket\n");
  
    printf("%s\n",buffer);
    seqn++;
 }
 
	close(sockfd);
  return 0;
}