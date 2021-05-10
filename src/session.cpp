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