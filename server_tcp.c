// codigo server_tcp.c fornecido pelo professor

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 4000

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

typedef struct __notification{
    uint32_t id; // Identificador da notificação (sugere-se um identificador único)
    uint32_t timestamp; // Timestamp da notificação
    const char* _string; // Mensagem
    uint16_t length; // Tamanho da mensagem
    uint16_t pending; // Quantidade de leitores pendentes
} notification;


void read_message(int newsockfd, char buffer)
{
	/* read from the socket */
    /*
    int n;
	n = read(newsockfd, buffer, 256);
	if (n < 0) 
		printf("ERROR reading from socket");
	printf("Here is the message: %s\n", buffer);
    */
}

void write_message(int newsockfd, char* message)
{
	/* write in the socket */
	//write_message(newsockfd, message)
    int n;
	n = write(newsockfd,"I got your message", 18);
	if (n < 0) 
		printf("ERROR writing to socket");
}

struct sockaddr_in setup_socket(int* sockfd)
{
    struct sockaddr_in serv_addr;

	if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket");
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);
    
	if (bind(*sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		printf("ERROR on binding");
    
    return serv_addr;
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, n;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	

    // setup socket
    //void setup_socket(int* sockfd)
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket");
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);     
    
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		printf("ERROR on binding");
	
	listen(sockfd, 5);
	
    
    // receive request / init connection
	clilen = sizeof(struct sockaddr_in);
	if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1) 
		printf("ERROR on accept");
	
	bzero(buffer, 256);

	
	/* read from the socket */
	//read_message(newsockfd, message_received);
	n = read(newsockfd, buffer, 256);
	if (n < 0) 
		printf("ERROR reading from socket");
	printf("Here is the message: %s\n", buffer);
	
	/* write in the socket */
	//write_message(newsockfd, message)
	n = write(newsockfd,"I got your message", 18);
	if (n < 0) 
		printf("ERROR writing to socket");

	close(newsockfd);
	close(sockfd);
	return 0; 
}