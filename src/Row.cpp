
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