
#pragma once

#include <list>
#include <string>
#include <string.h>
#include <map>
#include <fstream>
#include <iomanip>
#include <unistd.h>

#ifndef ROW_H
#define ROW_H
#include "../include/Row.hpp"
#endif

class MasterTable {
    protected:
        std::map< std::string, Row*> table;
        pthread_mutex_t read_write_mutex;
        pthread_mutex_t reader_mutex;
        pthread_mutex_t backup_table_mutex;
        int reader_counter;
    
    public:
    	MasterTable();
        void addUserIfNotExists(std::string username);
        int followUser(std::string followed, std::string follower);
        void sendMessageToFollowers(std::string username, std::string message);
        Row* getRow(std::string username);
        void load_backup_table();
        void save_backup_table();
        void deleteRows();
		void shared_reader_lock();
		void shared_reader_unlock();
};