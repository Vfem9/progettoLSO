#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/protocol.h"

Message parse_json_message(const char* json_string) {
    Message msg;
    memset(&msg, 0, sizeof(Message));

    const char* type_start = strstr(json_string, "\"type\":");
    if (type_start) {
        sscanf(type_start, "\"type\":\"%31[^\"]\"", msg.type);
    }

    const char* action_start = strstr(json_string, "\"action\":");
    if (action_start) {
        sscanf(action_start, "\"action\":\"%31[^\"]\"", msg.action);
    }

    const char* msg_id_start = strstr(json_string, "\"msg_id\":");
    if (msg_id_start) {
        sscanf(msg_id_start, "\"msg_id\":\"%63[^\"]\"", msg.msg_id);
    }

    const char* payload_start = strstr(json_string, "\"payload\":");
    if (payload_start) {
        const char* brace_start = strchr(payload_start, '{');
        if (brace_start) {
            int i = 0;
            const char* ptr = brace_start;
            int brace_count = 0;

            while (*ptr && i < 1023) {
                msg.payload[i++] = *ptr;
                if (*ptr == '{') brace_count++;
                if (*ptr == '}') {
                    brace_count--;
                    if (brace_count == 0) {
                        i++;
                        break;
                    }
                }
                ptr++;
            }
            msg.payload[i] = '\0';
        }
    }

    const char* error_start = strstr(json_string, "\"error\":");
    if (error_start) {
        sscanf(error_start, "\"error\":\"%255[^\"]\"", msg.error);
    }

    return msg;
}

char* message_to_json(const Message* msg) {
    char* json = (char*)malloc(2048);
    if (json == NULL) return NULL;

    memset(json, 0, 2048);

    snprintf(json, 2048,
        "{"
        "\"type\":\"%s\","
        "\"action\":\"%s\","
        "\"msg_id\":\"%s\","
        "\"payload\":%s,"
        "\"error\":\"%s\""
        "}",
        msg->type,
        msg->action,
        msg->msg_id,
        msg->payload[0] != '\0' ? msg->payload : "{}",
        msg->error
    );

    return json;
}
