
#pragma once

#include <algorithm>
#include <list>
#include <string>
#include <chrono>
#include <iostream>

class Row {
	// a row corresponds to a user

	protected:
		int active_sessions;
		bool notification_delivered;
		std::list<std::string> followers;
		std::list<std::string> messages_to_receive;
		int reader_counter;
		pthread_mutex_t read_write_mutex = PTHREAD_MUTEX_INITIALIZER;
		pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;

	public:
		Row();
		void startSession();
		void closeSession();
		int getActiveSessions();
		bool connectUser();
		bool get_notification_delivered();
		void set_notification_delivered(bool was_notification_delivered);
		std::list<std::string> getFollowers();
		bool hasFollower(std::string followerUsername);
		void setAddNewFollower(std::string username);
		void addNotification(std::string username, std::string message);
		// returns True if there is a notification
		bool hasNewNotification();
		// removes a notification from the list and return it
		std::string popNotification();
		// returns a notification from the list
		std::string getNotification();
		void shared_reader_lock();
		void shared_reader_unlock();


};
