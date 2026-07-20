#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/protocol.h"

// Parsa una stringa JSON semplice e la trasforma in struct Message
Message parse_json_message(const char* json_string) {
    Message msg;
    memset(&msg, 0, sizeof(Message));
    
    // Estrae il campo "type" dal JSON
    const char* type_start = strstr(json_string, "\"type\":");
    if (type_start) {
        sscanf(type_start, "\"type\":\"%31[^\"]\"", msg.type);
    }
    
    // Estrae il campo "action" dal JSON
    const char* action_start = strstr(json_string, "\"action\":");
    if (action_start) {
        sscanf(action_start, "\"action\":\"%31[^\"]\"", msg.action);
    }
    
    // Estrae il campo "msg_id" dal JSON
    const char* msg_id_start = strstr(json_string, "\"msg_id\":");
    if (msg_id_start) {
        sscanf(msg_id_start, "\"msg_id\":\"%63[^\"]\"", msg.msg_id);
    }
    
    // Estrae il campo "payload" dal JSON (tutto il contenuto tra le parentesi)
    const char* payload_start = strstr(json_string, "\"payload\":");
    if (payload_start) {
        // Trova l'inizio del payload
        const char* brace_start = strchr(payload_start, '{');
        if (brace_start) {
            // Estrae fino alla prossima virgola o fine stringa
            int i = 0;
            const char* ptr = brace_start;
            int brace_count = 0;
            
            // Copia il payload mantenendo le parentesi graffe
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
    
    // Estrae il campo "error" dal JSON
    const char* error_start = strstr(json_string, "\"error\":");
    if (error_start) {
        sscanf(error_start, "\"error\":\"%255[^\"]\"", msg.error);
    }
    
    return msg;
}

// Converte una struct Message in stringa JSON
char* message_to_json(const Message* msg) {
    char* json = (char*)malloc(2048);
    if (json == NULL) return NULL;
    
    memset(json, 0, 2048);
    
    // Costruisce il JSON aggiungendo campo per campo
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