#pragma once
#include <stdio.h>

typedef struct __session{
	int session_id;
	char* ip;
	int port;
    int seqn; // server seqn
    int last_received_seqn; // client seqn
} session_struct;

session_struct create_session(int session_id, char* ip, int port, int seqn, int last_received_seqn);

void print_session(session_struct packet);