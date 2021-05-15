#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>     /* atoi */

#ifndef PACKET_H
#define PACKET_H
#include "../include/packet.hpp"
#endif

typedef struct __session{
	int session_id;
	char* ip;
	int port;
    int seqn; // server seqn
    int last_received_seqn; // client seqn
} session_struct;

session_struct create_session(int session_id, char* ip, int port, int seqn, int last_received_seqn);

void print_session(session_struct packet);

void serialize_session(session_struct sessions_info, char* session_buffer);

session_struct unserialize_session(char* session_buffer);