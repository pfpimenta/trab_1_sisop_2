
#include "../include/MasterTable.hpp"

MasterTable	::MasterTable() {
	this->reader_counter = 0;
	this->read_write_mutex = PTHREAD_MUTEX_INITIALIZER;
    this->reader_mutex = PTHREAD_MUTEX_INITIALIZER;
    this->backup_table_mutex = PTHREAD_MUTEX_INITIALIZER;
}

void MasterTable::addUserIfNotExists(std::string username){
	Row* newRow = new Row;
	pthread_mutex_lock(&(this->read_write_mutex));
	bool usernameDoesNotExist = (this->table.find(username) == this->table.end());
	if(usernameDoesNotExist)
	{
		this->table.insert( std::make_pair( username, newRow) );
		this->save_backup_table();
	}
	pthread_mutex_unlock(&(this->read_write_mutex));
}

int MasterTable::followUser(std::string followed, std::string follower){
	
	pthread_mutex_lock(&(this->read_write_mutex));

	// check if current user exists 
	bool currentUserExists = (this->table.find(follower) != this->table.end());
	if(currentUserExists == false){
		pthread_mutex_unlock(&(this->read_write_mutex));
		return -1;
	}
	// check if newFollowing exists
	bool newFollowedExists = (this->table.find(followed) != this->table.end());
	if(newFollowedExists == false){
		pthread_mutex_unlock(&(this->read_write_mutex));
		return -1;
	}

	// check if currentUser is not trying to follow himself
	bool notFollowingHimself = (followed != follower);
	if(notFollowingHimself == false){
		pthread_mutex_unlock(&(this->read_write_mutex));
		return -2;
	}

	// check if currentUser does not follow newFollowing yet
	Row* followingRow = this->table.find(followed)->second;
	bool notDuplicateFollowing = (! followingRow->hasFollower(follower));
	if(notDuplicateFollowing == false) {
		pthread_mutex_unlock(&(this->read_write_mutex));
		return -3;
	} else {
		// add new follower!
		followingRow->setAddNewFollower(follower);
		this->save_backup_table();
		pthread_mutex_unlock(&(this->read_write_mutex));
		return 0;
	} 
}

void MasterTable::sendMessageToFollowers(std::string username, std::string message)
{
    this->shared_reader_lock();
    Row* currentRow = this->table.find(username)->second;
    std::list<std::string> followers = currentRow->getFollowers();
    for (std::string follower : followers){
        Row* followerRow = this->table.find(follower)->second;
        followerRow->addNotification(username, message);
    }
    this->shared_reader_unlock();
}

Row* MasterTable::getRow(std::string username){
	Row* currentRow;
	this->shared_reader_lock();
	currentRow = this->table.find(username)->second;
	this->shared_reader_unlock();
	return currentRow;
}

std::map< std::string, Row*> MasterTable::getTable(){
	return this->table;
}

// saves the master_table into a TXT file
void MasterTable::save_backup_table()
{
	pthread_mutex_lock(&(this->backup_table_mutex));
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
	pthread_mutex_unlock(&(this->backup_table_mutex));
}


// loads the master_table from a TXT file, if it exists
void MasterTable::load_backup_table()
{
	pthread_mutex_lock(&(this->backup_table_mutex));
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
			pthread_mutex_lock(&(this->read_write_mutex));
			this->table.insert( std::make_pair(username, row) );
			pthread_mutex_unlock(&(this->read_write_mutex));
		}
		table_file.close(); 
	} else {
		printf("Backup table not found. Creating new. \n");
		fflush(stdout);
	}
	pthread_mutex_unlock(&(this->backup_table_mutex));
}

void MasterTable::deleteRows()
{
	pthread_mutex_lock(&(this->backup_table_mutex));
    for (auto const& x : this->table) {
        delete(x.second);
    }
	pthread_mutex_unlock(&(this->backup_table_mutex));
}

void MasterTable::shared_reader_lock(){
	pthread_mutex_lock(&(this->reader_mutex));
	this->reader_counter++;
	if(this->reader_counter == 1){
		pthread_mutex_lock(&(this->read_write_mutex));
	}
	pthread_mutex_unlock(&(this->reader_mutex));
}

void MasterTable::shared_reader_unlock(){
	pthread_mutex_lock(&(this->reader_mutex));
	this->reader_counter--;
	if(this->reader_counter == 0){
		pthread_mutex_unlock(&(this->read_write_mutex));
	}
	pthread_mutex_unlock(&(this->reader_mutex));
}
