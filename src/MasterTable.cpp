
#include "../include/MasterTable.hpp"

// mutexes
pthread_mutex_t read_write_mutex_temp = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader_mutex_temp = PTHREAD_MUTEX_INITIALIZER;
int reader_counter_temp = 0;

// TODO maybe rename this
void shared_reader_lock_temp(){
	pthread_mutex_lock(&reader_mutex_temp);
	reader_counter_temp++;
	if(reader_counter_temp == 1){
		pthread_mutex_lock(&read_write_mutex_temp);
	}
	pthread_mutex_unlock(&reader_mutex_temp);
}

void shared_reader_unlock_temp(){
	pthread_mutex_lock(&reader_mutex_temp);
	reader_counter_temp--;
	if(reader_counter_temp == 0){
		pthread_mutex_unlock(&read_write_mutex_temp);
	}
	pthread_mutex_unlock(&reader_mutex_temp);
}


void MasterTable::addUserIfNotExists(std::string username){
	Row* newRow = new Row;
	pthread_mutex_lock(&read_write_mutex_temp);
	bool usernameDoesNotExist = (this->table.find(username) == this->table.end());
	if(usernameDoesNotExist)
	{
		this->table.insert( std::make_pair( username, newRow) );
		save_backup_table();
	}
	pthread_mutex_unlock(&read_write_mutex_temp);
}

int MasterTable::followUser(std::string followed, std::string follower){
	// TODO : mutexes
	
	// check if current user exists 
	bool currentUserExists = (this->table.find(follower) != this->table.end());
	if(currentUserExists == false){
		return -1;
	}
	// check if newFollowing exists
	bool newFollowedExists = (this->table.find(followed) != this->table.end());
	if(newFollowedExists == false){
		return -1;
	}

	// check if currentUser is not trying to follow himself
	bool notFollowingHimself = (followed != follower);
	if(notFollowingHimself == false){
		return -2;
	}

	// check if currentUser does not follow newFollowing yet
	Row* currentRow = this->table.find(follower)->second;
	Row* followingRow = this->table.find(followed)->second;
	bool notDuplicateFollowing = (! followingRow->hasFollower(follower));
	if(notDuplicateFollowing == false) {
		return -3;
	} else {
		// add new follower!
		followingRow->setAddNewFollower(follower);
		save_backup_table();
		return 0;
	} 
}

void MasterTable::sendMessageToFollowers(std::string username, std::string message)
{
    // TODO consertar mutexes
    shared_reader_lock_temp();
    Row* currentRow = this->table.find(username)->second;
    shared_reader_unlock_temp();
    std::list<std::string> followers = currentRow->getFollowers();
    for (std::string follower : followers){
        shared_reader_lock_temp();
        Row* followerRow = this->table.find(follower)->second;
        shared_reader_unlock_temp();
        followerRow->addNotification(username, message);
    }
}

Row* MasterTable::getRow(std::string username){
	Row* currentRow;
	shared_reader_lock_temp();
	currentRow = this->table.find(username)->second;
	shared_reader_unlock_temp();
	return currentRow;
}


// saves the master_table into a TXT file
void MasterTable::save_backup_table()
{
	//master_table
	std::list<std::string> followers;
	std::string username;
	Row* row;
	std::ofstream table_file;
	table_file.open ("backup_table.txt", std::ios::out | std::ios::trunc); 
	for(auto const& x : this->table)
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


// loads the master_table from a TXT file, if it exists
void MasterTable::load_backup_table()
{
	char* line_ptr_aux;
	char* token;
	Row* row;

	// if file exists
    const std::string& filename = "backup_table.txt";
	if(access( filename.c_str(), F_OK ) != -1 )
	{
		printf("Restoring backup... \n");
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
			pthread_mutex_lock(&read_write_mutex_temp);
			this->table.insert( std::make_pair(username, row) );
			pthread_mutex_unlock(&read_write_mutex_temp);
		}
		table_file.close(); 
	} else {
		printf("Backup table not found. Creating new. \n");
		fflush(stdout);
	}
}

void MasterTable::deleteRows()
{
    for (auto const& x : this->table) {
        delete(x.second);
    }
}