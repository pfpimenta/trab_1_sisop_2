
#pragma once

#include <algorithm>
#include <list>
#include <string>
#include <chrono>
#include <iostream>
#include <map>

#ifndef SESSION_H
#define SESSION_H
#include "../include/session.hpp"
#endif

#ifndef PACKET_H
#define PACKET_H
#include "../include/packet.hpp"
#endif

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
		std::map< int*, session_struct*> sessions;

	public:
		Row();
		// void startSession(); // TODO delete
		void closeSession(int session_id);
		int getActiveSessions();
		bool connectUser(session_struct new_session);
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
		void serialize_row(char* buffer, std::string username);
		std::string unserialize_row(char* buffer);
};
