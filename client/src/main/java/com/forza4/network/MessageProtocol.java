package com.forza4.network;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import java.util.concurrent.atomic.AtomicLong;

// Classe per parsare e serializzare i messaggi JSON del protocollo, e per le
// costanti condivise con il protocollo del server (protocol.h).
public class MessageProtocol {

    public static final String TYPE_REQUEST = "REQUEST";
    public static final String TYPE_RESPONSE = "RESPONSE";
    public static final String TYPE_NOTIFICATION = "NOTIFICATION";

    public static final String ACTION_REGISTER = "register";
    public static final String ACTION_CREATE_MATCH = "create_match";
    public static final String ACTION_LIST_MATCHES = "list_matches";
    // FIX: ACTION_JOIN_MATCH ora e' solo una RICHIESTA di partecipazione,
    // non un ingresso immediato in partita: il creatore deve accettarla
    // (ACTION_ACCEPT_JOIN) o rifiutarla (ACTION_REJECT_JOIN) esplicitamente.
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

    // FIX: prima non c'era un contatore univoco, i vari punti del codice
    // riusavano msg_id fissi ("msg_001" ecc.) per ogni richiesta dello stesso
    // tipo. Non rompeva il protocollo (il server si limita a echeggiare
    // msg_id), ma rendeva impossibile correlare richiesta/risposta lato client.
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

    // Crea un messaggio REQUEST con payload come oggetto JSON annidato (non stringa).
    // FIX: la versione precedente usava json.addProperty("payload", payload) con
    // payload di tipo String: Gson lo serializzava come STRINGA JSON con le
    // graffe escapate (es. "payload":"{\"match_id\":\"x\"}"), non come oggetto
    // annidato. Il server invece si aspetta "payload":{...} senza quote. Il
    // metodo non veniva mai usato proprio per questo (GameController costruiva
    // il JSON a mano); ora e' corretto e riutilizzabile.
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
