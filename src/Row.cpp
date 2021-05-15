
#include "../include/Row.hpp"


Row::Row(){
    this->active_sessions = 0;
    this->notification_delivered = false;
	this->reader_counter = 0;
}

void Row::closeSession(int session_id){
	pthread_mutex_lock(&(this->read_write_mutex));
	this->active_sessions -= 1;
	session_struct* session = this->sessions.find(&session_id)->second;
	free(session);
	this->sessions.erase(&session_id);
	pthread_mutex_unlock(&(this->read_write_mutex));
}

int Row::getActiveSessions(){
	this->shared_reader_lock();
	int num_active_sessions = this->active_sessions;
	this->shared_reader_unlock();
	return num_active_sessions;
}

bool Row::connectUser(session_struct new_session){
	bool connectionSuccessfull;
	pthread_mutex_lock(&(this->read_write_mutex));
	if(this->active_sessions >= 2){
		connectionSuccessfull = false;
	} else {
		this->active_sessions += 1;
		connectionSuccessfull = true;
		int* new_session_id = (int*)malloc(sizeof(int));
		*new_session_id = new_session.session_id;
		session_struct* new_session_ptr = (session_struct*)malloc(sizeof(session_struct));
		*new_session_ptr = new_session;
		this->sessions.insert( std::make_pair( new_session_id, new_session_ptr) );
	}
	pthread_mutex_unlock(&(this->read_write_mutex));
	return connectionSuccessfull;
}

bool Row::get_notification_delivered(){
	this->shared_reader_lock();
	bool notification_delivered = this->notification_delivered;
	this->shared_reader_unlock();
	return notification_delivered;
}

void Row::set_notification_delivered(bool was_notification_delivered){
	pthread_mutex_lock(&(this->read_write_mutex));
	this->notification_delivered = was_notification_delivered;
	pthread_mutex_unlock(&(this->read_write_mutex));
}

std::list<std::string> Row::getFollowers() {
	this->shared_reader_lock();
	std::list<std::string> followersList = this->followers;
	this->shared_reader_unlock();
	return followersList;
}

bool Row::hasFollower(std::string followerUsername) {
	this->shared_reader_lock();
	std::list<std::string>::iterator it;
	it = std::find(this->followers.begin(), this->followers.end(), followerUsername);
	bool found = it != this->followers.end();
	this->shared_reader_unlock();
	return found;
}

void Row::setAddNewFollower(std::string username) {
	pthread_mutex_lock(&(this->read_write_mutex));
	this->followers.push_back( username );
	fflush(stdout);
	pthread_mutex_unlock(&(this->read_write_mutex));
}

void Row::addNotification(std::string username, std::string message) {
	pthread_mutex_lock(&(this->read_write_mutex));

	auto now = std::chrono::system_clock::now();
	std::time_t now_time = std::chrono::system_clock::to_time_t(now);
	// first, generate payload string
	std::string payload = std::string(std::ctime(&now_time)) + " @" + username + ": " + message;

	std::cout << std::string(std::ctime(&now_time)) + " @" + username + ": " + message << std::endl;
	// put payload in list
	messages_to_receive.push_back(payload);
	pthread_mutex_unlock(&(this->read_write_mutex));
}

// returns True if there is a notification
bool Row::hasNewNotification(){
	this->shared_reader_lock();
	bool hasNotifications;
	if(!this->messages_to_receive.empty()) {
		hasNotifications = true;
	} else {
		hasNotifications= false;
	}
	this->shared_reader_unlock();
	return hasNotifications;
}

// removes a notification from the list and return it
std::string Row::popNotification() {
	pthread_mutex_lock(&(this->read_write_mutex));
	std::string notification = this->messages_to_receive.front();
	this->messages_to_receive.pop_front();
	pthread_mutex_unlock(&(this->read_write_mutex));
	this->set_notification_delivered(false);
	return notification;
}

// returns a notification from the list
std::string Row::getNotification() {
	this->shared_reader_lock();
	std::string notification = this->messages_to_receive.front();
	this->shared_reader_unlock();
	return notification;
}

// mutexes
void Row::shared_reader_lock(){
	pthread_mutex_lock(&(this->reader_mutex));
	this->reader_counter++;
	if(this->reader_counter == 1){
		pthread_mutex_lock(&(this->read_write_mutex));
	}
	pthread_mutex_unlock(&(this->reader_mutex));
}

void Row::shared_reader_unlock(){
	pthread_mutex_lock(&(this->reader_mutex));
	this->reader_counter--;
	if(this->reader_counter == 0){
		pthread_mutex_unlock(&(this->read_write_mutex));
	}
	pthread_mutex_unlock(&(this->reader_mutex));
}

void serialize_list(std::list<std::string> list, char* buffer) {
	// lista serializada:
	// <elem>$<elem>$...$<elem>
	bool is_first = true;
	char* aux_ptr = buffer;
	int amount_writen;
	memset(buffer, 0, SMALL_BUFFER_SIZE * sizeof(char));
	for (auto const& elem : list) {
		if(is_first){
			// printa sem virgula 
			amount_writen = snprintf(aux_ptr, BUFFER_SIZE, "%s", elem.c_str());
			is_first = false;
		} else {
			// printa com virgula 
			amount_writen = snprintf(aux_ptr, BUFFER_SIZE, "$%s", elem.c_str());
		}
		aux_ptr += amount_writen*sizeof(char);
	}
}

void serialize_map(std::map< int*, session_struct*> sessions_map, char* buffer) {
	// map serializado:
	// <session>$<session>$...$<session>
	bool is_first = true;
	char* aux_ptr = buffer;
	char* session_buffer = (char*)malloc(sizeof(char)*SMALL_BUFFER_SIZE);
	int amount_writen;
	memset(buffer, 0, SMALL_BUFFER_SIZE * sizeof(char));
	for (auto const& x : sessions_map) {
		// session_struct* session_infos_ptr = x.second;
		// session_struct sessions_info = *session_infos_ptr;
		session_struct sessions_info = *x.second;

		serialize_session(sessions_info, session_buffer);

		if(is_first){
			// printa sem virgula 
			amount_writen = snprintf(aux_ptr, BUFFER_SIZE, "%s", session_buffer);
			is_first = false;
		} else {
			// printa com virgula 
			amount_writen = snprintf(aux_ptr, BUFFER_SIZE, "$%s", session_buffer);
		}
		aux_ptr += amount_writen*sizeof(char);
	}
}

void Row::serialize_row(char* buffer, std::string username) {
	// Row serializada:
	// <username>#<active_sessions>#<notification_delivered>#<followers_buffer>#<messages_to_receive_buffer>#<sessions_buffer>

	char* followers_buffer = (char*)malloc(sizeof(char)*SMALL_BUFFER_SIZE);
	char* messages_to_receive_buffer = (char*)malloc(sizeof(char)*SMALL_BUFFER_SIZE);
	char* sessions_buffer = (char*)malloc(sizeof(char)*SMALL_BUFFER_SIZE);
 
	// serialize followers_buffer
	serialize_list(this->followers, followers_buffer);
	std::cout << "DEBUG followers_buffer: " << followers_buffer << std::endl;

	// serialize messages_to_receive_buffer
	serialize_list(this->messages_to_receive, messages_to_receive_buffer);
	std::cout << "DEBUG messages_to_receive_buffer: " << messages_to_receive_buffer << std::endl;
	
	// serialize sessions_buffer
	serialize_map(this->sessions, sessions_buffer);
	std::cout << "DEBUG sessions_buffer: " << sessions_buffer << std::endl;

	memset(buffer, 0, BUFFER_SIZE * sizeof(char));
	snprintf(buffer, BUFFER_SIZE, "%s#%d#%d#%s#%s#%s\n",
		username.c_str(),
		this->active_sessions,
		this->notification_delivered,
		followers_buffer,
		messages_to_receive_buffer,
		sessions_buffer
	);
	
	std::cout << "DEBUG row buffer: " << buffer << std::endl;
}

std::list<std::string> unserialize_list(char* buffer) {
	// lista serializada:
	// <elem>$<elem>$...$<elem>
	std::list<std::string>  parsed_list;
	char* list_buffer = (char*)malloc(sizeof(char)*SMALL_BUFFER_SIZE);
	strcpy(list_buffer, buffer);
	// parse
	char* token;
	const char delimiter[2] = "$";
	char* rest = list_buffer;
	int payload_size = strlen(list_buffer);
	list_buffer[payload_size] = '\0';
	while((token = strtok_r(rest, delimiter, &rest)) != NULL){
		// parse element		
		std::string parsed_element(token);
		parsed_list.push_back(parsed_element);
	}

	return parsed_list;
}

std::map< int*, session_struct*> unserialize_map(char* buffer) {
	// map serializado:
	// <session>$<session>$...$<session>
	std::map< int*, session_struct*> parsed_map;
	char* map_buffer = (char*)malloc(sizeof(char)*SMALL_BUFFER_SIZE);
	char* session_buffer = (char*)malloc(sizeof(char)*SMALL_BUFFER_SIZE);
	strcpy(map_buffer, buffer);
	// parse
	char* token;
	const char delimiter[2] = "$";
	char* rest = map_buffer;
	int payload_size = strlen(map_buffer);
	map_buffer[payload_size] = '\0';
	while((token = strtok_r(rest, delimiter, &rest)) != NULL){
		// parse session
		strcpy(session_buffer, buffer);
		session_struct parsed_session = unserialize_session(session_buffer);
		session_struct* session_ptr = (session_struct*)malloc(sizeof(session_struct));
		*session_ptr = parsed_session;
		int* id_ptr = (int*)malloc(sizeof(int));
		*id_ptr = parsed_session.session_id;
		parsed_map.insert(std::make_pair(id_ptr, session_ptr));
	}

	return parsed_map;
}


// saves the Row information in the buffer in this Row object
// returns the username
std::string Row::unserialize_row(char* buffer){
	// Row serializada:
	// <username>#<active_sessions>#<notification_delivered>#<followers_buffer>#<messages_to_receive_buffer>#<sessions_buffer>

	// parse
	char* token;
	const char delimiter[2] = "#";
	char* rest = buffer;
	int payload_size = strlen(buffer);
	buffer[payload_size] = '\0';
	// parse username
	token = strtok_r(rest, delimiter, &rest);
	std::string username(token);
	// parse active_sessions
	token = strtok_r(rest, delimiter, &rest);
  	this->active_sessions = atoi(token);
	// parse notification_delivered
	token = strtok_r(rest, delimiter, &rest);
  	this->notification_delivered = (bool)atoi(token);
	// parse followers
	token = strtok_r(rest, delimiter, &rest);
	this->followers = unserialize_list(token);
	// parse messages_to_receive
	token = strtok_r(rest, delimiter, &rest);
	this->messages_to_receive = unserialize_list(token);
	// parse sessions
	token = strtok_r(rest, delimiter, &rest);
	this->sessions = unserialize_map(token);


	return username;	
}