
#include "../include/Row.hpp"

// mutexes
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

Row::Row(){
    this->active_sessions = 0;
    this->notification_delivered = false;
}

		void Row::startSession(){
			pthread_mutex_lock(&read_write_mutex);
			this->active_sessions += 1;
			pthread_mutex_unlock(&read_write_mutex);
		}

		void Row::closeSession(){
			pthread_mutex_lock(&read_write_mutex);
			this->active_sessions -= 1;
			pthread_mutex_unlock(&read_write_mutex);
		}

		int Row::getActiveSessions(){
			shared_reader_lock();
			int num_active_sessions = this->active_sessions;
			shared_reader_unlock();
			return num_active_sessions;
		}

		bool Row::get_notification_delivered(){
			shared_reader_lock();
			bool notification_delivered = this->notification_delivered;
			shared_reader_unlock();
			return notification_delivered;
		}

		void Row::set_notification_delivered(bool was_notification_delivered){
			pthread_mutex_lock(&read_write_mutex);
			this->notification_delivered = was_notification_delivered;
			pthread_mutex_unlock(&read_write_mutex);
		}

		std::list<std::string> Row::getFollowers() {
			shared_reader_lock();
			std::list<std::string> followersList = this->followers;
			shared_reader_unlock();
			return followersList;
		}

		bool Row::hasFollower(std::string followerUsername) {
			shared_reader_lock();
			std::list<std::string>::iterator it;
			it = std::find(this->followers.begin(), this->followers.end(), followerUsername);
			bool found = it != this->followers.end();
			shared_reader_unlock();
			return found;
		}

		void Row::setAddNewFollower(std::string username) {
			pthread_mutex_lock(&read_write_mutex);
			this->followers.push_back( username );
			fflush(stdout);
			pthread_mutex_unlock(&read_write_mutex);
		}

		void Row::addNotification(std::string username, std::string message) {
			pthread_mutex_lock(&read_write_mutex);

			auto now = std::chrono::system_clock::now();
			std::time_t now_time = std::chrono::system_clock::to_time_t(now);
			// first, generate payload string
			std::string payload = std::string(std::ctime(&now_time)) + " @" + username + ": " + message;

			std::cout << std::string(std::ctime(&now_time)) + " @" + username + ": " + message << std::endl;
			// put payload in list
			messages_to_receive.push_back(payload);
			pthread_mutex_unlock(&read_write_mutex);
		}

		// returns True if there is a notification
		bool Row::hasNewNotification(){
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
		std::string Row::popNotification() {
			shared_reader_lock();
			std::string notification = this->messages_to_receive.front();
			this->messages_to_receive.pop_front();
			shared_reader_unlock();
			this->set_notification_delivered(false);
			return notification;
		}

		// returns a notification from the list
		std::string Row::getNotification() {
			shared_reader_lock();
			std::string notification = this->messages_to_receive.front();
			shared_reader_unlock();
			return notification;
		}
