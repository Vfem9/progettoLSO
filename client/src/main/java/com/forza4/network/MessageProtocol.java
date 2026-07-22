package com.forza4.network;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import java.util.concurrent.atomic.AtomicLong;


public class MessageProtocol {

    public static final String TYPE_REQUEST = "REQUEST";
    public static final String TYPE_RESPONSE = "RESPONSE";
    public static final String TYPE_NOTIFICATION = "NOTIFICATION";

    public static final String ACTION_REGISTER = "register";
    public static final String ACTION_CREATE_MATCH = "create_match";
    public static final String ACTION_LIST_MATCHES = "list_matches";

    // RICHIESTA di partecipazione,
    public static final String ACTION_JOIN_MATCH = "join_match";
    public static final String ACTION_ACCEPT_JOIN = "accept_join";
    public static final String ACTION_REJECT_JOIN = "reject_join";
    public static final String ACTION_CANCEL_JOIN = "cancel_join";
    public static final String ACTION_MOVE = "move";
    public static final String ACTION_REMATCH = "rematch";
    public static final String ACTION_QUIT_MATCH = "quit_match";
    public static final String ACTION_MATCH_STARTED = "match_started";
    public static final String ACTION_OPPONENT_LEFT = "opponent_left";
    public static final String ACTION_MATCH_CLOSED = "match_closed";
    // Notifiche legate al nuovo flusso di richiesta/accettazione:
    public static final String ACTION_JOIN_REQUEST = "join_request";                   // al creatore: qualcuno vuole partecipare
    public static final String ACTION_JOIN_REQUEST_REJECTED = "join_request_rejected"; // al richiedente: rifiutato
    public static final String ACTION_JOIN_REQUEST_OBSOLETE = "join_request_obsolete"; // al richiedente: partita non piu' disponibile
    public static final String ACTION_JOIN_REQUEST_PROMOTED = "join_request_promoted"; // a chi era in coda: la sua richiesta e' ora in decisione
    public static final String ACTION_MATCH_UNAVAILABLE = "match_unavailable"; // broadcast: una partita non e' piu' disponibile (avviata da altri)

    private static final AtomicLong MSG_COUNTER = new AtomicLong(0);

    private static String nextMsgId() {
        return "msg_" + System.currentTimeMillis() + "_" + MSG_COUNTER.incrementAndGet();
    }

    public static JsonObject parseJson(String jsonString) {
        try {
            JsonElement element = JsonParser.parseString(jsonString);
            return element.getAsJsonObject();
        } catch (Exception e) {
            System.err.println("Errore parsing JSON: " + e.getMessage());
            return null;
        }
    }

    public static String getMessageType(JsonObject json) {
        if (json != null && json.has("type")) {
            return json.get("type").getAsString();
        }
        return null;
    }

    public static String getAction(JsonObject json) {
        if (json != null && json.has("action")) {
            return json.get("action").getAsString();
        }
        return null;
    }

    public static String getError(JsonObject json) {
        if (json != null && json.has("error") && !json.get("error").isJsonNull()) {
            return json.get("error").getAsString();
        }
        return "";
    }

    public static JsonObject getPayload(JsonObject json) {
        if (json != null && json.has("payload") && json.get("payload").isJsonObject()) {
            return json.get("payload").getAsJsonObject();
        }
        return null;
    }

    public static String getPayloadString(JsonObject json, String key) {
        JsonObject payload = getPayload(json);
        if (payload != null && payload.has(key)) {
            return payload.get(key).getAsString();
        }
        return null;
    }

    public static int getPayloadInt(JsonObject json, String key) {
        JsonObject payload = getPayload(json);
        if (payload != null && payload.has(key)) {
            return payload.get(key).getAsInt();
        }
        return -1;
    }

    public static String createRequestMessage(String action, JsonObject payload) {
        JsonObject json = new JsonObject();
        json.addProperty("type", TYPE_REQUEST);
        json.addProperty("action", action);
        json.addProperty("msg_id", nextMsgId());
        json.add("payload", payload != null ? payload : new JsonObject());
        json.addProperty("error", "");

        return json.toString();
    }

    public static String createRequestMessage(String action) {
        return createRequestMessage(action, new JsonObject());
    }
}
