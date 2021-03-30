// codigo server_tcp.c fornecido pelo professor

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>


#define PORT 4000
#define MAX_THREADS 15 // maximum number of threads allowed
#define BUFFER_SIZE 256
#define MESSAGE_SIZE 128

int seqn = 0;

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

typedef struct __notification{
    uint32_t id; // Identificador da notificação (sugere-se um identificador único)
    uint32_t timestamp; // Timestamp da notificação
    const char* _string; // Mensagem
    uint16_t length; // Tamanho da mensagem
    uint16_t pending; // Quantidade de leitores pendentes
} notification;

// reads a message from a socket (receives a message through it)
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

// writes a message in a socket (sends a message through it)
void write_message(int newsockfd, char* message)
{
	/* write in the socket */
    int n;
	n = write(newsockfd, message, strlen(message));
	if (n < 0) 
		printf("ERROR writing to socket");
}

// opens a socket, binds it, and starts listening for incoming connections
int setup_socket()
{
	int sockfd;
    struct sockaddr_in serv_addr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
	{
        printf("ERROR opening socket");
        exit(-1);
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);
    
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
	{
		printf("ERROR on binding");
        exit(-1);
	}

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

// serializes the packet and puts it in the buffer
void serialize_packet(packet packet_to_send, char* buffer)
{
  bzero(buffer, sizeof(buffer));
  snprintf(buffer, BUFFER_SIZE, "%u,%u,%u,%s",
          packet_to_send.seqn, packet_to_send.length, packet_to_send.type, packet_to_send._payload);
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


// controls the client inputs received by socket
void * socket_thread(void *arg) {
	int socket = *((int *)arg);
	int size = 0;
	int reference_seqn;
	char payload[MESSAGE_SIZE];
	char client_message[BUFFER_SIZE];
	char reply[BUFFER_SIZE];
	packet packet_to_send;

	// print pthread id
	pthread_t thread_id = pthread_self();
	printf("Socket opened in thread %d\n", (int)thread_id);

	do{
		// receive message
  		bzero(client_message, sizeof(client_message));
		size = recv(socket, client_message, BUFFER_SIZE, 0); 
		client_message[size] = '\0';
		printf("Thread %d - Received message: %s\n", (int)thread_id, client_message);

		// TODO : treat received message
		// usar strtok
		// fazer um switch case

		// send ACK
		reference_seqn = 0; // TODO
  		bzero(payload, sizeof(payload));
		snprintf(payload, MESSAGE_SIZE, "%d", reference_seqn);
		packet_to_send = create_packet(payload, 4);
		serialize_packet(packet_to_send, reply);
		printf("Thread %d - Sending message: %s\n", (int)thread_id, reply);
		write_message(socket, reply);
  	}while (size != 0);

	printf("Exiting socket thread: %d\n", (int)thread_id);
	close(socket);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int sockfd;
	int newsockfd, n;
	socklen_t clilen;
	char buffer[BUFFER_SIZE];
  	char message[MESSAGE_SIZE];
	struct sockaddr_in serv_addr, cli_addr;
	packet packet_to_send;
  	pthread_t tid[MAX_THREADS];

    // setup main socket
    sockfd = setup_socket();

	// loop that accepts new connections and allocates threads to deal with them
    int i = 0;
	while(1) {
		// accept new connection in a new socket
		clilen = sizeof(struct sockaddr_in);
		if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1) 
			printf("ERROR on accept");

		// create thread for the new socket
		if (pthread_create(&tid[i], NULL, socket_thread, &newsockfd) != 0 ) {
			printf("Failed to create thread\n");
			exit(-1);
		}

		// TODO: codigo pra limitar o numero maximo de threads
		// e fechar threads que ja terminaram
	}

	close(sockfd);
	return 0; 
}