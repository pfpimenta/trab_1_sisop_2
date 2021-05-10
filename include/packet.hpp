#pragma once

#include <stdio.h>
#include <string>
#include <string.h>

#define BUFFER_SIZE 256
#define PAYLOAD_SIZE 128

#define TYPE_CONNECT 0
#define TYPE_FOLLOW 1
#define TYPE_SEND 2
#define TYPE_MSG 3
#define TYPE_ACK 4
#define TYPE_ERROR 5
#define TYPE_DISCONNECT 6
#define TYPE_SET_ID 7
#define TYPE_UPDATE_ROW 8
#define TYPE_HEARTBEAT 9
#define TYPE_BACKUP 10


typedef struct __packet{
    uint16_t type; // Tipo do pacote:
        // 0 - CONNECT (username_to_login, seqn)
        // 1 - FOLLOW (username_to_follow, seqn)
        // 2 - SEND (message_to_send, seqn)
        // 3 - MSG (username, message_sent, seqn)
        // 4 - ACK (seqn)
        // 5 - ERROR (seqn, error_message)
        // 6 - DISCONNECT (seqn)
        // 7 - SET_ID (id, seqn)
        // 8 - UPDATE_ROW (Row, seqn)
        // 9 - HEARTBEAT (seqn)
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