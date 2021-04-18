
#pragma once

#include <list>
#include <string>
#include <string.h>
#include <map>
#include <fstream>
#include <iomanip>
#include <unistd.h>

#include "../include/Row.hpp"

class MasterTable {
    protected:
        std::map< std::string, Row*> table;
    
    public:
        void addUserIfNotExists(std::string username);
        int followUser(std::string followed, std::string follower);
        void sendMessageToFollowers(std::string username, std::string message);
        Row* getRow(std::string username);
        void load_backup_table();
        void save_backup_table();
        void deleteRows();
};