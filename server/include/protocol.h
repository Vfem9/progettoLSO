#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MSG_TYPE_REQUEST        "REQUEST"
#define MSG_TYPE_RESPONSE       "RESPONSE"
#define MSG_TYPE_NOTIFICATION   "NOTIFICATION"

#define ACTION_REGISTER         "register"
#define ACTION_CREATE_MATCH     "create_match"
#define ACTION_LIST_MATCHES     "list_matches"
#define ACTION_JOIN_MATCH       "join_match"
#define ACTION_ACCEPT_JOIN      "accept_join"
#define ACTION_REJECT_JOIN      "reject_join"
#define ACTION_MOVE             "move"
#define ACTION_QUIT_MATCH       "quit_match"

typedef struct {
    char type[32];
    char action[32];
    char msg_id[64];
    char payload[1024];
    char error[256];
} Message;

Message parse_json_message(const char* json_string);
char* message_to_json(const Message* msg);

#endif