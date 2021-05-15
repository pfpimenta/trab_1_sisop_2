#include "../include/session.hpp"

session_struct create_session(int session_id, char* ip, int port, int seqn, int last_received_seqn)
{
  session_struct new_session;
  new_session.session_id = session_id;
  new_session.ip = ip;
  new_session.port = port;
  new_session.seqn = seqn;
  new_session.last_received_seqn = last_received_seqn;
  return new_session;
}

void print_session(session_struct new_session)
{
    printf("\nSession id: %i \n", new_session.session_id);
    printf("IP: %s \n", new_session.ip);
    printf("port: %i \n", new_session.port);
    printf("seqn: %i \n", new_session.seqn);
    printf("last_received_seqn: %i\n", new_session.last_received_seqn);
    fflush(stdout);
}

// serializes the session_struct and puts it in the buffer
void serialize_session(session_struct sessions_info, char* session_buffer) {
  memset(session_buffer, 0, 600 * sizeof(char));
  snprintf(session_buffer, 600, "%d&%s&%d&%d&%d",
          sessions_info.session_id,
          sessions_info.ip,
          sessions_info.port,
          sessions_info.seqn,
          sessions_info.last_received_seqn);
}

// unserializes the session_struct and puts it in the buffer
session_struct unserialize_session(char* session_buffer) {
  session_struct sessions_info;
  // parse
  char* token;
  const char delimiter[2] = "&";
  char* rest = session_buffer;
  int payload_size = strlen(session_buffer);
  rest = '\0';
  // parse session_id
  token = strtok_r(rest, delimiter, &rest);
  sessions_info.session_id = atoi(token);
  // parse ip
  token = strtok_r(rest, delimiter, &rest);
  strcpy(sessions_info.ip, token);
  // parse port
  token = strtok_r(rest, delimiter, &rest);
  sessions_info.port = atoi(token);
  // parse seqn
  token = strtok_r(rest, delimiter, &rest);
  sessions_info.seqn = atoi(token);
  // parse last_received_seqn
  token = strtok_r(rest, delimiter, &rest);
  sessions_info.last_received_seqn = atoi(token);
}
