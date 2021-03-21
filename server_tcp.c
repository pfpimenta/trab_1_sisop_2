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

// writes a message from a socket (receives a message through it)
void read_message(int newsockfd, char* buffer)
{
	/* read from the socket */
    int n;
	n = read(newsockfd, buffer, 256);
	if (n < 0) 
		printf("ERROR reading from socket");
}

// writes a message in a socket (sends a message through it)
void write_message(int newsockfd, char* message)
{
	/* write in the socket */
    int n;
	n = write(newsockfd,"I got your message", 18);
	if (n < 0) 
		printf("ERROR writing to socket");
}

// opens a socket, binds it, and starts listening for incoming connections
int setup_socket()
{
	int sockfd;
    struct sockaddr_in serv_addr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket");
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);
    
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		printf("ERROR on binding");
    
	listen(sockfd, 5);

    return sockfd;
}

int accept_connection(int sockfd)
{
	int newsockfd; // socket created for the connection
	socklen_t clilen;
	struct sockaddr_in cli_addr;

	clilen = sizeof(struct sockaddr_in);
	if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1) 
		printf("ERROR on accept");

	return newsockfd;
}


int main(int argc, char *argv[])
{
	int sockfd;
	int newsockfd, n;
	// socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	
    // setup socket
    sockfd = setup_socket();
    
    // receive request / init connection
	newsockfd = accept_connection(sockfd);

	// make sure buffer is clear	
	bzero(buffer, 256);

	/* read from the socket */
	read_message(newsockfd, buffer);
	printf("Here is the message: %s\n", buffer);
	
	/* write in the socket */
	strcpy(buffer, "I got your message");
	write_message(newsockfd, buffer);

	close(newsockfd);
	close(sockfd);
	return 0; 
}