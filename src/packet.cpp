#include "../include/packet.hpp"

packet create_packet(char* message, int packet_type, int seqn)
{
  packet packet_to_send;
  packet_to_send._payload = message;
  packet_to_send.seqn = seqn;
  packet_to_send.length = strlen(packet_to_send._payload);
  packet_to_send.type = packet_type;
  return packet_to_send;
}

// serializes the packet and puts it in the buffer
void serialize_packet(packet packet_to_send, char* buffer)
{
  memset(buffer, 0, (packet_to_send.length + 30)* sizeof(char));
  snprintf(buffer, BUFFER_SIZE, "%u,%u,%u,%s\n",
          packet_to_send.seqn, packet_to_send.length, packet_to_send.type, packet_to_send._payload);
}

void print_packet(packet packet) {
  printf("Reference seqn: %i \n", packet.seqn);
  printf("Payload length: %i \n", packet.length);
  printf("Packet type: %i - %s\n", packet.type, get_packet_type_string(packet.type));
  printf("Payload: %s \n\n", packet._payload);
  fflush(stdout);
}

packet buffer_to_packet(char* buffer) {
  packet packet;
  char payload[PAYLOAD_SIZE];

  int buffer_size = strlen(buffer);
  buffer[buffer_size] = '\0';

  char* token;
  const char delimiter[2] = ",";
  char* rest = buffer;
  
  //seqn
  token = strtok_r(rest, delimiter, &rest);
  packet.seqn = atoi(token);

  //payload_length
  token = strtok_r(rest, delimiter, &rest);
  packet.length = atoi(token);

  //packet_type
  token = strtok_r(rest, delimiter, &rest);
  packet.type = atoi(token);

  //payload (get whatever else is in there)
  bzero(payload, PAYLOAD_SIZE); //clear payload buffer
  token = strtok_r(rest, "", &rest);
  strncpy(payload, token, packet.length);
  payload[packet.length] = '\0';
  packet._payload = (char*) malloc((packet.length) * sizeof(char) + 1);
  memcpy(packet._payload, payload, (packet.length) * sizeof(char) + 1);

  return packet;
}

const char* get_packet_type_string(int packet_type)
{
  switch (packet_type)
  {
  case 0:
    return "CONNECT";
    break;
  case 1:
    return "FOLLOW";
    break;
  case 2:
    return "SEND";
    break;
  case 3:
    return "MSG";
    break;
  case 4:
    return "ACK";
    break;
  case 5:
    return "ERROR";
    break;
  case 6:
    return "DISCONNECT";
    break;
  case 7:
    return "SET_ID";
    break;
  case 8:
    return "UPDATE_ROW";
    break;
  case 9:
    return "UPDATE_BACKUP";
    break;
  case 10:
    return "HEARTBEAT";
    break;
  case 11:
    return "SERVER_CHANGE";
    break;
  case 12:
    return "CONNECT_SERVER";
    break;
  default:
    printf("ERROR: invalid packet type\n");
    return NULL;
    break;
  }
}

int get_packet_type(char* packet_type_string)
{

  if (strcmp(packet_type_string, "CONNECT") == 0) 
  {
    return 0;
  } 
  else if (strcmp(packet_type_string, "FOLLOW") == 0)
  {
    return 1;
  }
  else if (strcmp(packet_type_string, "SEND") == 0)
  {
    return 2;
  }
  else if (strcmp(packet_type_string, "MSG") == 0)
  {
    return 3;
  }
  else if (strcmp(packet_type_string, "ACK") == 0)
  {
    return 4;
  }
  else if (strcmp(packet_type_string, "ERROR") == 0)
  {
    return 5;
  }
  else if (strcmp(packet_type_string, "DISCONNECT") == 0)
  {
    return 6;
  }
  else
  {
		printf("ERROR: unkown packet type\n");
    exit(0);
  }
}