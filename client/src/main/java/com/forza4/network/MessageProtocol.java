package com.forza4.network;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

// Classe per parsare e serializzare i messaggi JSON
public class MessageProtocol {
    
    // Parsa una stringa JSON in una JsonObject
    public static JsonObject parseJson(String jsonString) {
        try {
            JsonElement element = JsonParser.parseString(jsonString);
            return element.getAsJsonObject();
        } catch (Exception e) {
            System.err.println("Errore parsing JSON: " + e.getMessage());
            return null;
        }
    }
    
    // Estrae il tipo di messaggio
    public static String getMessageType(JsonObject json) {
        if (json != null && json.has("type")) {
            return json.get("type").getAsString();
        }
        return null;
    }
    
    // Estrae l'azione
    public static String getAction(JsonObject json) {
        if (json != null && json.has("action")) {
            return json.get("action").getAsString();
        }
        return null;
    }
    
    // Estrae il payload
    public static JsonObject getPayload(JsonObject json) {
        if (json != null && json.has("payload")) {
            return json.get("payload").getAsJsonObject();
        }
        return null;
    }
    
    // Estrae un valore string dal payload
    public static String getPayloadString(JsonObject json, String key) {
        JsonObject payload = getPayload(json);
        if (payload != null && payload.has(key)) {
            return payload.get(key).getAsString();
        }
        return null;
    }
    
    // Estrae un valore int dal payload
    public static int getPayloadInt(JsonObject json, String key) {
        JsonObject payload = getPayload(json);
        if (payload != null && payload.has(key)) {
            return payload.get(key).getAsInt();
        }
        return -1;
    }
    
    // Crea un messaggio REQUEST
    public static String createRequestMessage(String action, String payload) {
        JsonObject json = new JsonObject();
        json.addProperty("type", "REQUEST");
        json.addProperty("action", action);
        json.addProperty("msg_id", "msg_" + System.currentTimeMillis());
        json.addProperty("payload", payload);
        json.addProperty("error", "");
        
        return json.toString();
    }
    
    // Crea un messaggio RESPONSE
    public static String createResponseMessage(String action, String payload) {
        JsonObject json = new JsonObject();
        json.addProperty("type", "RESPONSE");
        json.addProperty("action", action);
        json.addProperty("msg_id", "msg_" + System.currentTimeMillis());
        json.addProperty("payload", payload);
        json.addProperty("error", "");
        
        return json.toString();
    }
}
