// codigo server_tcp.c fornecido pelo professor

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <utility>
#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include <pthread.h>
#include <csignal>
#include <algorithm>
#include <iterator>
#include <ctime>

pthread_mutex_t read_write_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;
int reader_counter = 0;

void shared_reader_lock(){
	pthread_mutex_lock(&reader_mutex);
	reader_counter++;
	if(reader_counter == 1){
		pthread_mutex_lock(&read_write_mutex);
	}
	pthread_mutex_unlock(&reader_mutex);
}

void shared_reader_unlock(){
	pthread_mutex_lock(&reader_mutex);
	reader_counter--;
	if(reader_counter == 0){
		pthread_mutex_unlock(&read_write_mutex);
	}
	pthread_mutex_unlock(&reader_mutex);
}

#define PORT 4000
#define MAX_THREADS 30 // maximum number of threads allowed
#define BUFFER_SIZE 256
#define PAYLOAD_SIZE 128

#define TYPE_CONNECT 0
#define TYPE_FOLLOW 1
#define TYPE_SEND 2
#define TYPE_MSG 3
#define TYPE_ACK 4
#define TYPE_ERROR 5
#define TYPE_DISCONNECT 6

int seqn = 0;

// Global termination flag, set by the signal handler.
// TODO add mutexes for get/set
bool termination_signal = false;

//-- Master table --
class Row {
	// a row corresponds to a user

	protected:
		int active_sessions;
		bool notification_delivered;
		std::list<std::string> followers;
		std::list<std::string> messages_to_receive;

	public:
		//constructor
		Row(){
			this->active_sessions = 0;
			this->notification_delivered = false;
		};

		void startSession(){
			pthread_mutex_lock(&read_write_mutex);
			this->active_sessions += 1;
			pthread_mutex_unlock(&read_write_mutex);
		}

		void closeSession(){
			pthread_mutex_lock(&read_write_mutex);
			this->active_sessions -= 1;
			pthread_mutex_unlock(&read_write_mutex);
		}

		int getActiveSessions(){
			shared_reader_lock();
			int num_active_sessions = this->active_sessions;
			shared_reader_unlock();
			return num_active_sessions;
		}

		bool get_notification_delivered(){
			shared_reader_lock();
			bool notification_delivered = this->notification_delivered;
			shared_reader_unlock();
			return notification_delivered;
		}

		void set_notification_delivered(bool was_notification_delivered){
			pthread_mutex_lock(&read_write_mutex);
			this->notification_delivered = was_notification_delivered;
			pthread_mutex_unlock(&read_write_mutex);
		}

		std::list<std::string> getFollowers() {
			shared_reader_lock();
			std::list<std::string> followersList = this->followers;
			shared_reader_unlock();
			return followersList;
		}

		bool hasFollower(std::string followerUsername) {
			shared_reader_lock();
			std::list<std::string>::iterator it;
			it = std::find(this->followers.begin(), this->followers.end(), followerUsername);
			bool found = it != this->followers.end();
			shared_reader_unlock();
			return found;
		}

		void setAddNewFollower(std::string username) {
			pthread_mutex_lock(&read_write_mutex);
			this->followers.push_back( username );
			fflush(stdout);
			pthread_mutex_unlock(&read_write_mutex);
		}

		void addNotification(std::string username, std::string message) {
			pthread_mutex_lock(&read_write_mutex);
			// first, generate payload string
			std::string payload = "@" + username + ": " + message;
			// put payload in list
			messages_to_receive.push_back(payload);
			pthread_mutex_unlock(&read_write_mutex);
		}

		// returns True if there is a notification
		bool hasNewNotification(){
			shared_reader_lock();
			bool hasNotifications;
			if(!this->messages_to_receive.empty()) {
				hasNotifications = true;
			} else {
				hasNotifications= false;
			}
			shared_reader_unlock();
			return hasNotifications;
		}

		// removes a notification from the list and return it
		std::string popNotification() {
			shared_reader_lock();
			std::string notification = this->messages_to_receive.front();
			this->messages_to_receive.pop_front();
			shared_reader_unlock();
			this->set_notification_delivered(false);
			return notification;
		}

		// returns a notification from the list
		std::string getNotification() {
			shared_reader_lock();
			std::string notification = this->messages_to_receive.front();
			shared_reader_unlock();
			return notification;
		}
};
typedef std::map< std::string, Row*> master_table_t;

master_table_t master_table; //Globally accessible master table instance

//------------------

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


// saves the master_table into a TXT file
void save_backup_table()
{
	//master_table
	std::list<std::string> followers;
	std::string username;
	Row* row;
	std::ofstream table_file;
	table_file.open ("backup_table.txt", std::ios::out | std::ios::trunc); 
	// TODO precisa de um lock aqui? tem q ver como botar sem dar conflito com getFollowers
	for(auto const& x : master_table)
	{	
		username = x.first;
		row = x.second;
		followers = row->getFollowers();

		table_file << username;
		table_file << ".";
		
		for (std::string follower : followers)
		{
			table_file << follower;
			table_file << ",";
		}
		table_file << "\n";
	}
	table_file.close(); 
}

inline bool file_exists (const std::string& name) {
    return ( access( name.c_str(), F_OK ) != -1 );
}

// loads the master_table from a TXT file, if it exists
master_table_t load_backup_table()
{
	char* line_ptr_aux;
	char* token;
	Row* row;

	// if file exists
	if(file_exists("backup_table.txt"))
	{
		printf("Loading backup table... \n");
		fflush(stdout);
		std::ifstream table_file("backup_table.txt");
		for( std::string line; getline( table_file, line ); )
		{
			char* line_ptr = strdup(line.c_str());

			row = new Row;
			strcpy(line_ptr, line.c_str());

			token = strtok_r(line_ptr, ".", &line_ptr_aux);
			std::string username(token);

			token = strtok_r(NULL, ",", &line_ptr_aux);
			while(token != NULL)
			{
				std::string follower(token);
				row->setAddNewFollower(follower);
				token = strtok_r(NULL, ",", &line_ptr_aux);
			}

			// insert new (usename, row) in master_table
			pthread_mutex_lock(&read_write_mutex);
			master_table.insert( std::make_pair(username, row) );
			pthread_mutex_unlock(&read_write_mutex);
		}
		table_file.close(); 
	} else {
		printf("Backup table not found. Creating new. \n");
		fflush(stdout);
	}
	return master_table;
}

void closeConnection(int socket, int thread_id)
{
	printf("Closing connection and exiting socket thread: %d\n", thread_id);
	if (shutdown(socket, SHUT_RDWR) !=0 ) {
		std::cout << "Failed to shutdown a connection socket." << std::endl;
	}
	close(socket);
	pthread_exit(NULL);
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

	if ((sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) 
	{
        printf("ERROR creating LISTEN socket");
        exit(-1);
	}

	int enable = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    	printf("setsockopt(SO_REUSEADDR) failed");
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);
    
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) 
	{
		printf("ERROR on binding LISTEN socket");
        exit(-1);
	}

	if (listen(sockfd, 50) != 0) {
		printf("ERROR on activating LISTEN socket");
        exit(-1);
	}

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
  bzero(buffer, BUFFER_SIZE * sizeof(char));
  snprintf(buffer, BUFFER_SIZE, "%u,%u,%u,%s\n",
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


// Thread designated for the connected client
void * socket_thread(void *arg) {
	int socket = *((int *)arg);
	int size = 0;
	int reference_seqn = 0;
	char payload[PAYLOAD_SIZE];
	char client_message[BUFFER_SIZE];
	char buffer[BUFFER_SIZE];
	int message_type = -1;
	int payload_length = -1;
	Row* currentRow;
	std::string currentUser = "not-connected";
	std::string notification;
	
	packet packet_to_send;

	// print pthread id
	pthread_t thread_id = pthread_self();
	printf("Socket opened in thread %d\n", (int)thread_id);

	std::cout << "DEBUG: This thread is trying to use this socket: " << socket << std::endl;

	int flags = fcntl(socket, F_GETFL);
	if (flags == -1) {
		printf("FAILED getting socket flags.\n");
	}

	flags = flags | O_NONBLOCK;
	if (fcntl(socket, F_SETFL, flags) != 0) {
		printf("FAILED setting socket to NON-BLOCKING mode.\n");
	}
	// zero-fill the buffer
	bzero(client_message, sizeof(client_message));

	do{
		// receive message
		size = recv(socket, client_message, BUFFER_SIZE-1, 0);
		if (size > 0) {
			bzero(payload, PAYLOAD_SIZE); //clear payload buffer

			client_message[size] = '\0';
			printf("Thread %d - Read buffer: %s (size: %d)\n", (int)thread_id, client_message, (int)size);
			fflush(stdout);

			// parse socket buffer: get several messages, if there are more than one
			char* token_end_of_packet;
  			char* rest_packet = client_message;
			while((token_end_of_packet = strtok_r(rest_packet, "\n", &rest_packet)) != NULL)
			{

				strcpy(buffer, token_end_of_packet); // put token_end_of_packet in buffer
				char* token;
				char* rest = buffer;
				const char delimiter[2] = ",";

				//seqn
				token = strtok_r(rest, delimiter, &rest);
				reference_seqn = atoi(token);

				//payload_length
				token = strtok_r(rest, delimiter, &rest);
				payload_length = atoi(token);

				//packet_type
				token = strtok_r(rest, delimiter, &rest);
				message_type = atoi(token);

				//payload (get whatever else is in there up to '\n')
				token = strtok_r(rest, "\n", &rest);
				strncpy(payload, token, payload_length);

				printf(" Reference seqn: %i \n Payload length: %i \n Packet type: %i \n Payload: %s \n", reference_seqn, payload_length, message_type, payload);
				fflush(stdout);

				switch (message_type) {
					case TYPE_CONNECT:
					{
						std::string Username(payload); //copying char array into proper std::string type
						currentUser = Username;
						Row* newRow = new Row;

						// check if map already has the username in there before inserting
						shared_reader_lock();
						bool usernameDoesNotExist = (master_table.find(Username) == master_table.end());
						shared_reader_unlock();
						if(usernameDoesNotExist)
						{
							pthread_mutex_lock(&read_write_mutex);
							master_table.insert( std::make_pair( Username, newRow) );
							pthread_mutex_unlock(&read_write_mutex);
							save_backup_table();
						}


						// checks if there are already 2 open sessions for this user
						shared_reader_lock();
						currentRow = master_table.find(currentUser)->second;
						shared_reader_unlock();
						int activeSessions = currentRow->getActiveSessions();
						if(activeSessions >= 2){
							printf("ERROR: there are already 2 active sessions!\n");
							// TODO send ERROR packet to client
							closeConnection(socket, (int)thread_id);
						} else if (activeSessions == 1) {
							currentRow->startSession();
						} else {
							currentRow->startSession();
						}
						break;
					}
					case TYPE_FOLLOW:
					{
						std::string newFollowingUsername(payload); //copying char array into proper std::string type
						
						// check if current user exists 
						shared_reader_lock();
						bool currentUserExists = (master_table.find(currentUser) != master_table.end());
						// check if newFollowing exists
						bool newFollowingExists = (master_table.find(newFollowingUsername) != master_table.end());
						// check if currentUser is not trying to follow himself
						bool notFollowingHimself = (currentUser != newFollowingUsername);
						shared_reader_unlock();
						if(currentUserExists && newFollowingExists && notFollowingHimself)
						{
							shared_reader_lock();
							currentRow = master_table.find(currentUser)->second;
							Row* followingRow = master_table.find(newFollowingUsername)->second;
							// check if currentUser does not follow newFollowing yet
							bool notDuplicateFollowing = (! followingRow->hasFollower(currentUser));
							shared_reader_unlock();
							if(notDuplicateFollowing) {
								followingRow->setAddNewFollower(currentUser);
							} else {
								printf("ERROR: client user is already following this other user!\n");
								fflush(stdout);
								// TODO send ERROR packet to client
							}
							save_backup_table();
						} else {
							printf("ERROR: one of the users does not exist OR user tried to follow himself!\n");
							fflush(stdout);
							// TODO send ERROR packet to client
						}
						break;
					}
					case TYPE_SEND:
					{
						// check if current user exists
						shared_reader_lock();
						bool cond = (master_table.find(currentUser) != master_table.end());
						shared_reader_unlock();
						if(cond)
						{
							shared_reader_lock();
							currentRow = master_table.find(currentUser)->second;
							shared_reader_unlock();
							std::string message(payload); //copying char array into proper std::string type
							std::list<std::string> followers = currentRow->getFollowers();
							for (std::string follower : followers){
								shared_reader_lock();
								Row* followerRow = master_table.find(follower)->second;
								shared_reader_unlock();
								followerRow->addNotification(currentUser, message);
							}
						} else {
							printf("ERROR: user does not exist!\n");
							fflush(stdout);
						}
						break;
					}
					case TYPE_DISCONNECT:
					{
						currentRow = master_table.find(currentUser)->second;
						printf("Exiting socket thread: %d\n", (int)thread_id);
						currentRow->closeSession();
						if (shutdown(socket, SHUT_RDWR) !=0 ) {
							std::cout << "Failed to shutdown a connection socket." << std::endl;
						}
						close(socket);
						pthread_exit(NULL);
						break;
					}
				}
				// send ACK
				/*reference_seqn = 0; // TODO
				bzero(payload, sizeof(payload));
				snprintf(payload, PAYLOAD_SIZE, "%d", reference_seqn);
				packet_to_send = create_packet(payload, 4);
				serialize_packet(packet_to_send, reply);
				printf("Thread %d - Sending message: %s\n", (int)thread_id, reply);
				write_message(socket, reply);*/
			}
		}


		// send message, if there is any
		if(currentUser != "not-connected") // if the user has already connected
		{
			shared_reader_lock();
			currentRow = master_table.find(currentUser)->second;
			shared_reader_unlock();			

			// TODO validar essa parte:
			if(currentRow->hasNewNotification())
			{
				int activeSessions = currentRow->getActiveSessions();
				// TODO mutex for the if
				if(activeSessions == 1) {
					// consume notification and remove it from the FIFO
					notification = currentRow->popNotification();
					strcpy(payload, notification.c_str());
					packet_to_send = create_packet(payload, 0);
					serialize_packet(packet_to_send, buffer);
					write_message(socket, buffer);
				} else if(activeSessions == 2) {
					bool was_notification_delivered = currentRow->get_notification_delivered();
					if(was_notification_delivered) {
						// consume notification and remove it from the FIFO
						notification = currentRow->popNotification();
						strcpy(payload, notification.c_str());
						packet_to_send = create_packet(payload, 0);
						serialize_packet(packet_to_send, buffer);
						write_message(socket, buffer);
					} else {
						// consume notification but DO NOT remove it from the FIFO
						notification = currentRow->getNotification();
						strcpy(payload, notification.c_str());
						packet_to_send = create_packet(payload, 0);
						serialize_packet(packet_to_send, buffer);
						write_message(socket, buffer);
						currentRow->set_notification_delivered(true);
						// TODO wait for notification_delivered == false
						bool timeout_condition = true;
						std::time_t start_timestamp = std::time(nullptr);
						while(currentRow->get_notification_delivered() == true && timeout_condition){
							std::time_t loop_timestamp = std::time(nullptr);
							bool timeout_condition = (loop_timestamp - start_timestamp <= 3);
							// TODO ver se eh em segundos
						}
					}
				}
				// TODO receive ACK
			}


			// 	// TODO receive ACK
			// }
		}
  	}while (termination_signal == false);

	printf("Exiting socket thread: %d\n", (int)thread_id);
	currentRow->closeSession();
	if (shutdown(socket, SHUT_RDWR) !=0 ) {
		std::cout << "Failed to shutdown a connection socket." << std::endl;
	}
	close(socket);
	pthread_exit(NULL);
}

void exit_hook_handler(int signal) {
	std::cout<< std::endl << "Signal received: code is " << signal << std::endl;

	termination_signal = true; //TODO mutex for set
}

int main(int argc, char *argv[])
{
	int i = 0;
	int sockfd;
	int newsockfd;
	socklen_t clilen;
	char buffer[BUFFER_SIZE];
  	char message[PAYLOAD_SIZE];
	struct sockaddr_in serv_addr, cli_addr;
	packet packet_to_send;
	std::list<pthread_t> threads_list;
	pthread_t newthread;

	// Install handler (assign handler to signal)
    std::signal(SIGINT, exit_hook_handler);
	std::signal(SIGTERM, exit_hook_handler);
	std::signal(SIGABRT, exit_hook_handler);

	// load backup table
	master_table = load_backup_table();

	// setup LISTEN socket
    sockfd = setup_socket();
	printf("LISTEN socket open and ready. \n");

	clilen = sizeof(struct sockaddr_in);

	// loop that accepts new connections and allocates threads to deal with them
	while(1) {
		
		// If new incoming connection, accept. Then continue as normal.
		if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) != -1) {
			if (pthread_create(&newthread, NULL, socket_thread, &newsockfd) != 0 ) {
				printf("Failed to create thread\n");
				exit(-1);
			} else {
				threads_list.push_back(newthread);
				std::cout << "DEBUG: Spawned thread with ID " << (int)newthread << " should be using socket " << newsockfd << std::endl;
			}
		} /*else {
			std::cout << "Failed accepting new incoming connection." << std::endl;
		}*/

		// Cleanup code for main server thread
		if(termination_signal == true) {
			if (shutdown(sockfd, SHUT_RDWR) != 0 ) {
				std::cout << "Failed to shutdown a connection socket." << std::endl;
			}
			close(sockfd);
			for (auto const& i : threads_list) {
				pthread_join(i, NULL);
			}

			// TODO: free em tudo que foi alocado?

			std::cout << "Exiting..." << std::endl;
			exit(0);
		}
	}
}
