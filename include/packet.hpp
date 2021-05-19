#pragma once

#include <stdio.h>
#include <string>
#include <string.h>

#define PAYLOAD_SIZE 2000
#define BUFFER_SIZE PAYLOAD_SIZE+50
#define SMALL_BUFFER_SIZE (BUFFER_SIZE/10)

#define TYPE_CONNECT 0
#define TYPE_FOLLOW 1
#define TYPE_SEND 2
#define TYPE_MSG 3
#define TYPE_ACK 4
#define TYPE_ERROR 5
#define TYPE_DISCONNECT 6
#define TYPE_SET_ID 7
#define TYPE_UPDATE_ROW 8
#define TYPE_UPDATE_BACKUP 9
#define TYPE_HEARTBEAT 10
#define TYPE_SERVER_CHANGE 11
#define TYPE_CONNECT_SERVER 12


typedef struct __packet{
    uint16_t type; // Tipo do pacote:
        // 0 - CONNECT (username_to_login, port)
        // 1 - FOLLOW (username_to_follow)
        // 2 - SEND (message_to_send)
        // 3 - MSG (username, message_sent)
        // 4 - ACK
        // 5 - ERROR (error_message)
        // 6 - DISCONNECT
        // 7 - SET_ID (id)
        // 8 - UPDATE_ROW (Row)
        // 9 - UPDATE_BACKUP (server_struct)
        // 10 - HEARTBEAT
        // 11 - SERVER_CHANGE (ip, port)
        // 12 - CONNECT_SERVER (port, backup_id)
    uint16_t seqn; // Número de sequência
    uint16_t length; // Comprimento do payload
    char* _payload; // Dados da mensagem
} packet;

packet create_packet(char* message, int packet_type, int seqn);

// serializes the packet and puts it in the buffer
void serialize_packet(packet packet_to_send, char* buffer);

void print_packet(packet packet);

packet buffer_to_packet(char* buffer);

const char* get_packet_type_string(int packet_type);

int get_packet_type(char* packet_type_string);