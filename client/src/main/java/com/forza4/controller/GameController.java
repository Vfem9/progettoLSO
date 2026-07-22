package com.forza4.controller;

import com.forza4.model.GameModel;
import com.forza4.network.MessageProtocol;
import com.forza4.network.NetworkClient;
import com.forza4.view.LobbyPanel;
import com.forza4.view.MainFrame;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;

import javax.swing.JDialog;
import javax.swing.JOptionPane;
import javax.swing.SwingUtilities;

public class GameController implements NetworkClient.MessageListener {
    private NetworkClient networkClient;
    private GameModel gameModel;
    private LobbyPanel lobbyPanel;
    private MainFrame mainFrame;

    // Riferimento al popup "Hai perso, in attesa dell'avversario" mostrato a
    // chi ha perso. Quando arriva la vera decisione
    // dell'avversario (rivincita o abbandono), possiamo chiuderlo noi stessi
    // invece di lasciare che si accumuli un secondo dialog sopra.
    private JDialog waitingDialog;

    // Popup "il creatore sta valutando un'altra richiesta" mostrato a chi finisce in coda.
    private JDialog queuedInfoDialog;

    // Inizializza modello, connessione di rete e collega il controller alla lobby.
    public GameController(LobbyPanel lobbyPanel) {
        this.lobbyPanel = lobbyPanel;
        this.gameModel = new GameModel();

        this.networkClient = new NetworkClient("localhost", 5000, this);

        this.lobbyPanel.setController(this);
    }

    public void setMainFrame(MainFrame mainFrame) {
        this.mainFrame = mainFrame;
    }

    // Il client registra l'username scelto dal giocatore e ottiene un player_id dal server.
    public void connectToServer(String username) {
        try {
            networkClient.connect();
            System.out.println("Connesso al server!");

            gameModel.setPlayerUsername(username);
            register(username);

        } catch (Exception e) {
            System.err.println("Errore connessione: " + e.getMessage());
            SwingUtilities.invokeLater(() ->
                JOptionPane.showMessageDialog(mainFrame,
                    "Impossibile connettersi al server: " + e.getMessage(),
                    "Errore di connessione", JOptionPane.ERROR_MESSAGE));
        }
    }

    private void register(String username) {
        JsonObject payload = new JsonObject();
        payload.addProperty("username", username);
        networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_REGISTER, payload));
    }

    public void createMatch() {
        networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_CREATE_MATCH));
    }

    public void joinMatch(String matchId) {
        JsonObject payload = new JsonObject();
        payload.addProperty("match_id", matchId);
        networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_JOIN_MATCH, payload));
    }

    public void listMatches() {
        networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_LIST_MATCHES));
    }

    public void makeMove(String matchId, int column) {
        if (!gameModel.isMyTurn()) {
            System.out.println("Non e' il tuo turno, mossa ignorata lato client.");
            return;
        }

        JsonObject payload = new JsonObject();
        payload.addProperty("match_id", matchId);
        payload.addProperty("column", column);
        networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_MOVE, payload));
    }

    public void requestRematch() {
        String matchId = gameModel.getCurrentMatchId();
        if (matchId == null) return;

        JsonObject payload = new JsonObject();
        payload.addProperty("match_id", matchId);
        networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_REMATCH, payload));
    }

    public void returnToLobby() {
        gameModel.setMatchState(GameModel.STATE_NONE);
        gameModel.setCurrentMatchId(null);
        if (mainFrame != null) {
            mainFrame.switchToLobbyPanel();
        }
        listMatches();
    }

    @Override
    public void onMessageReceived(String rawMessage) {
        SwingUtilities.invokeLater(() -> handleMessage(rawMessage));
    }

    private void handleMessage(String rawMessage) {
        System.out.println("Messaggio ricevuto dal server: " + rawMessage);

        JsonObject json = MessageProtocol.parseJson(rawMessage);
        if (json == null) {
            return;
        }

        String type = MessageProtocol.getMessageType(json);
        String action = MessageProtocol.getAction(json);
        String error = MessageProtocol.getError(json);

        if (type == null || action == null) {
            return;
        }

        System.out.println("Type: " + type + ", Action: " + action);

        if (!error.isEmpty()) {
            handleError(action, error);
            return;
        }

        JsonObject payload = MessageProtocol.getPayload(json);

        if (MessageProtocol.TYPE_RESPONSE.equals(type)) {
            handleResponse(action, payload);
        } else if (MessageProtocol.TYPE_NOTIFICATION.equals(type)) {
            handleNotification(action, payload);
        }
    }

    private void handleError(String action, String error) {
        System.err.println("Errore dal server (" + action + "): " + error);
        JOptionPane.showMessageDialog(mainFrame, error, "Errore", JOptionPane.WARNING_MESSAGE);
    }

    private void handleResponse(String action, JsonObject payload) {
        if (payload == null) return;

        if (MessageProtocol.ACTION_REGISTER.equals(action)) {
            int playerId = payload.has("player_id") ? payload.get("player_id").getAsInt() : -1;
            gameModel.setMyPlayerId(playerId);
            System.out.println("Registrato con player_id = " + playerId);
            // Controllo per evitare che un giocatore avvii più partite contemporaneamente.
            lobbyPanel.setInteractionEnabled(true);
            listMatches();
        }
        else if (MessageProtocol.ACTION_LIST_MATCHES.equals(action)) {
            lobbyPanel.clearMatches();
            if (payload.has("matches")) {
                JsonArray matches = payload.get("matches").getAsJsonArray();
                for (int i = 0; i < matches.size(); i++) {
                    JsonObject match = matches.get(i).getAsJsonObject();
                    String matchId = match.get("match_id").getAsString();
                    // Vengono mostrati gli username scelti dai due giocatori in partita.
                    String creatorUsername = match.has("creator_username")
                        ? match.get("creator_username").getAsString()
                        : String.valueOf(match.get("creator").getAsInt());
                    String status = match.get("status").getAsString();

                    String matchInfo = matchId + " (Creator: " + creatorUsername + ", Status: " + status + ")";
                    lobbyPanel.addMatch(matchInfo);
                }
            }
        }
        else if (MessageProtocol.ACTION_CREATE_MATCH.equals(action)) {
            applyMatchState(payload, 1);
            gameModel.setMatchState(GameModel.STATE_WAITING);
            if (mainFrame != null) {
                mainFrame.switchToGamePanel();
            }
        }
        else if (MessageProtocol.ACTION_JOIN_MATCH.equals(action)) {
            // Schermata di attesa (JoinWaitingPanel) per chi ha richiesto di partecipare, in attesa che il creatore accetti o rifiuti. 
            String status = payload.has("status") ? payload.get("status").getAsString() : "";
            String creatorUsername = payload.has("creator_username") ? payload.get("creator_username").getAsString() : "Il creatore";
            String matchId = payload.has("match_id") ? payload.get("match_id").getAsString() : null;

            gameModel.setCurrentMatchId(matchId);
            gameModel.setOpponentUsername(creatorUsername);
            gameModel.setMatchState(GameModel.STATE_JOIN_PENDING);

            if (mainFrame != null) {
                mainFrame.switchToJoinWaitingPanel();
                if (mainFrame.getJoinWaitingPanel() != null) {
                    mainFrame.getJoinWaitingPanel().setWaitingText(
                        "In attesa che " + creatorUsername + " risponda alla tua richiesta di partecipare...");
                }
            }

            if ("queued".equals(status)) {
                JOptionPane pane = new JOptionPane(
                    creatorUsername + " sta valutando un'altra richiesta.\nLa tua richiesta e' in coda.",
                    JOptionPane.INFORMATION_MESSAGE);
                queuedInfoDialog = pane.createDialog(mainFrame, "Richiesta in coda");
                queuedInfoDialog.setVisible(true);
                queuedInfoDialog = null;
            }
        }

        else if (MessageProtocol.ACTION_REMATCH.equals(action)) {
            // Chi vince decide se aprire una nuova sessione o meno. Chi perde puo' solo accettare o rifiutare la rivincita.
            String status = payload.has("status") ? payload.get("status").getAsString() : "ok";
            if ("new_session".equals(status)) {
                gameModel.setOpponentId(-1);
                gameModel.setOpponentUsername(null);
                applyMatchState(payload, 1);
                gameModel.setMatchState(GameModel.STATE_WAITING);
                if (mainFrame != null) {
                    mainFrame.switchToGamePanel();
                }
            } else if ("waiting_for_opponent_vote".equals(status)) {
                JOptionPane.showMessageDialog(mainFrame,
                    "Hai accettato la rivincita.\nIn attesa che anche l'avversario accetti...",
                    "In attesa", JOptionPane.INFORMATION_MESSAGE);
            }
        }
    }

    private void handleNotification(String action, JsonObject payload) {
        if (payload == null) return;

        if (MessageProtocol.ACTION_MATCH_STARTED.equals(action)) {
            // Se chi ha perso stava ancora guardando il popup "in attesa
            // dell'avversario" e l'avversario ha appena avviato una
            // rivincita, lo chiudiamo
            closeWaitingDialogIfOpen();
            closeQueuedInfoDialogIfOpen();

            // Gestione notifiche per chi ha creato la partita e per chi ha richiesto di partecipare.
            boolean wasCreatorWaiting = gameModel.getMatchState() == GameModel.STATE_WAITING;
            boolean wasJoinPending = gameModel.getMatchState() == GameModel.STATE_JOIN_PENDING;

            // Notifica il creatore che l'avversario si è unito alla partita.
            int playerNumber = payload.has("player_number") ? payload.get("player_number").getAsInt() : gameModel.getMyPlayerNumber();
            applyMatchState(payload, playerNumber);
            gameModel.setMatchState(GameModel.STATE_ACTIVE);

            if (mainFrame != null) {
                mainFrame.switchToGamePanel();
                if (mainFrame.getGamePanel() != null) {
                    mainFrame.getGamePanel().repaint();
                    String opponentUsername = gameModel.getOpponentUsername();
                    if (wasCreatorWaiting && opponentUsername != null && !opponentUsername.isEmpty()) {
                        mainFrame.getGamePanel().showTemporaryBanner(
                            opponentUsername + " si e' unito alla partita!");
                    } else if (wasJoinPending && opponentUsername != null && !opponentUsername.isEmpty()) {
                        mainFrame.getGamePanel().showTemporaryBanner(
                            "Ti sei unito alla partita di " + opponentUsername + "!");
                    }
                }
            }
        }
        else if (MessageProtocol.ACTION_JOIN_REQUEST.equals(action)) {
            // Il creatore può accettare o rifiutare le richeste di partecipazione.
            String requesterUsername = payload.has("requester_username") ? payload.get("requester_username").getAsString() : "Un giocatore";
            String matchId = payload.has("match_id") ? payload.get("match_id").getAsString() : gameModel.getCurrentMatchId();

            int choice = JOptionPane.showConfirmDialog(mainFrame,
                requesterUsername + " vuole partecipare, accetti?",
                "Richiesta di partecipazione", JOptionPane.YES_NO_OPTION);

            JsonObject respPayload = new JsonObject();
            respPayload.addProperty("match_id", matchId);
            if (choice == JOptionPane.YES_OPTION) {
                networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_ACCEPT_JOIN, respPayload));
            } else {
                networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_REJECT_JOIN, respPayload));
            }
        }
        else if (MessageProtocol.ACTION_JOIN_REQUEST_REJECTED.equals(action)) {
            // Notifica che il creatore ha rifiutato la richiesta di partecipazione.
            closeQueuedInfoDialogIfOpen();
            String creatorUsername = payload.has("creator_username") ? payload.get("creator_username").getAsString() : "Il creatore";
            JOptionPane.showMessageDialog(mainFrame,
                creatorUsername + " ha rifiutato la tua richiesta di partecipare.",
                "Richiesta rifiutata", JOptionPane.INFORMATION_MESSAGE);
            returnToLobby();
        }
        else if (MessageProtocol.ACTION_JOIN_REQUEST_OBSOLETE.equals(action)) {
            // Notifica che la partita a cui si voleva partecipare non è più disponibile (perché l'avversario ha iniziato la partita con qualcun altro o l'ha chiusa).
            closeQueuedInfoDialogIfOpen();
            String reason = payload.has("reason") ? payload.get("reason").getAsString() : "closed";
            String message = "started".equals(reason)
                ? "La partita a cui volevi partecipare e' gia' iniziata con un altro giocatore."
                : "La partita non e' piu' disponibile.";
            JOptionPane.showMessageDialog(mainFrame, message, "Partita non disponibile", JOptionPane.INFORMATION_MESSAGE);
            returnToLobby();
        }
        else if (MessageProtocol.ACTION_MATCH_UNAVAILABLE.equals(action)) {
            // Aggiorna la lista dei match.
            listMatches();
        }
        else if (MessageProtocol.ACTION_JOIN_REQUEST_PROMOTED.equals(action)) {

            closeQueuedInfoDialogIfOpen();
        }
        else if (MessageProtocol.ACTION_MOVE.equals(action)) {
            int[][] board = parseBoard(payload);
            gameModel.setBoard(board);

            if (payload.has("current_turn")) {
                gameModel.setCurrentTurn(payload.get("current_turn").getAsInt());
            }

            // Controlla se la partita è finita (vittoria, sconfitta o pareggio).
            String status = payload.has("status") ? payload.get("status").getAsString() : "ok";
            boolean gameOver = "win".equals(status) || "draw".equals(status);
            if (gameOver) {
                int winnerId = payload.has("winner_id") ? payload.get("winner_id").getAsInt() : -1;
                gameModel.setMatchState(GameModel.STATE_FINISHED);
                gameModel.setWinnerId(winnerId);
            }

            if (mainFrame != null && mainFrame.getGamePanel() != null) {
                mainFrame.getGamePanel().repaint();
            }

            if (gameOver) {
                showGameOverDialog("draw".equals(status));
            }
        }
        else if (MessageProtocol.ACTION_OPPONENT_LEFT.equals(action)) {
            // Gestione abbandono partita da parte dell'avversario (sia durante la partita che in attesa di una rivincita).
            closeWaitingDialogIfOpen();
            gameModel.setMatchState(GameModel.STATE_FINISHED);
            gameModel.setWinnerId(gameModel.getMyPlayerId());
            String reason = payload.has("reason") ? payload.get("reason").getAsString() : "disconnect";
            String message = "left".equals(reason)
                ? "Il tuo avversario ha abbandonato la partita. Hai vinto a tavolino!"
                : "Il tuo avversario si e' disconnesso. Hai vinto a tavolino!";
            JOptionPane.showMessageDialog(mainFrame, message,
                "Avversario assente", JOptionPane.INFORMATION_MESSAGE);
            returnToLobby();
        }
        else if (MessageProtocol.ACTION_MATCH_CLOSED.equals(action)) {
            // Gestione chiusura partita da parte dell'avversario (sia durante la partita che in attesa di una rivincita).
            closeWaitingDialogIfOpen();
            if (gameModel.getMatchState() == GameModel.STATE_FINISHED) {
                JOptionPane.showMessageDialog(mainFrame,
                    "L'avversario ha lasciato la partita.",
                    "Partita chiusa", JOptionPane.INFORMATION_MESSAGE);
                returnToLobby();
            }
        }
    }

    // Chiude il popup "in attesa dell'avversario" mostrato a chi ha perso, se e' ancora aperto. Serve per evitare che si accumulino due dialog uno sopra l'altro quando l'avversario avvia una rivincita.
    private void closeWaitingDialogIfOpen() {
        if (waitingDialog != null && waitingDialog.isShowing()) {
            waitingDialog.dispose();
        }
        waitingDialog = null;
    }


    private void closeQueuedInfoDialogIfOpen() {
        if (queuedInfoDialog != null && queuedInfoDialog.isShowing()) {
            queuedInfoDialog.dispose();
        }
        queuedInfoDialog = null;
    }

    // Applica match_id/player_number/opponent_id/board/current_turn al model.
    // Usato da create_match, join_match e match_started (prima partita e rivincite).
    private void applyMatchState(JsonObject payload, int fallbackPlayerNumber) {
        if (payload.has("match_id")) {
            gameModel.setCurrentMatchId(payload.get("match_id").getAsString());
        }
        gameModel.setMyPlayerNumber(payload.has("player_number") ? payload.get("player_number").getAsInt() : fallbackPlayerNumber);
        if (payload.has("opponent_id")) {
            gameModel.setOpponentId(payload.get("opponent_id").getAsInt());
        }

        if (payload.has("opponent_username")) {
            gameModel.setOpponentUsername(payload.get("opponent_username").getAsString());
        }
        gameModel.setBoard(parseBoard(payload));
        if (payload.has("current_turn")) {
            gameModel.setCurrentTurn(payload.get("current_turn").getAsInt());
        }
        gameModel.setWinnerId(-1);
    }

    private int[][] parseBoard(JsonObject payload) {
        int[][] board = new int[6][7];
        if (payload != null && payload.has("board") && payload.get("board").isJsonArray()) {
            JsonArray rows = payload.get("board").getAsJsonArray();
            for (int r = 0; r < rows.size() && r < 6; r++) {
                JsonArray row = rows.get(r).getAsJsonArray();
                for (int c = 0; c < row.size() && c < 7; c++) {
                    board[r][c] = row.get(c).getAsInt();
                }
            }
        }
        return board;
    }

    // Dialog di fine partita. Solo il vincitore può decidere se aprire una nuova sessione o meno. Il perdente può solo accettare o rifiutare la rivincita.
    private void showGameOverDialog(boolean isDraw) {
        boolean iWon = gameModel.amIWinner();

        if (isDraw) {
            int choice = JOptionPane.showConfirmDialog(mainFrame,
                "Pareggio!\nVuoi proporre una nuova partita?\n(serve che anche l'avversario sia d'accordo)",
                "Partita terminata", JOptionPane.YES_NO_OPTION);
            if (choice == JOptionPane.YES_OPTION) {
                requestRematch();
            } else {

                leaveMatch();
            }
        } else if (iWon) {
            int choice = JOptionPane.showConfirmDialog(mainFrame,
                "Hai vinto!\nVuoi aprire una nuova sessione? Ne diventerai il proprietario"
                    + " e sara' visibile a tutti in lobby (non necessariamente allo stesso avversario).",
                "Partita terminata", JOptionPane.YES_NO_OPTION);
            if (choice == JOptionPane.YES_OPTION) {
                requestRematch();
            } else {
                leaveMatch();
            }
        } else {
            JOptionPane.showMessageDialog(mainFrame,
                "Hai perso.", "Partita terminata", JOptionPane.INFORMATION_MESSAGE);
            leaveMatch();
        }
    }

    // Avviso al server che il giocatore vuole abbandonare la partita. Poi torna in lobby.
    public void leaveMatch() {
        String matchId = gameModel.getCurrentMatchId();
        if (matchId != null) {
            JsonObject payload = new JsonObject();
            payload.addProperty("match_id", matchId);
            networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_QUIT_MATCH, payload));
        }
        returnToLobby();
    }

    // Metodo "Annulla Richiesta" per chi ha richiesto di partecipare a una partita e si trova in attesa della risposta del creatore. Invia un messaggio al server per annullare la richiesta e torna in lobby.
    public void cancelJoinRequest() {
        closeQueuedInfoDialogIfOpen();
        String matchId = gameModel.getCurrentMatchId();
        if (matchId != null) {
            JsonObject payload = new JsonObject();
            payload.addProperty("match_id", matchId);
            networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_CANCEL_JOIN, payload));
        }
        returnToLobby();
    }

    @Override
    public void onConnectionClosed() {
        SwingUtilities.invokeLater(() -> {
            System.out.println("Disconnesso dal server");
            JOptionPane.showMessageDialog(mainFrame,
                "Connessione al server persa.", "Disconnesso", JOptionPane.ERROR_MESSAGE);
        });
    }

    @Override
    public void onError(String error) {
        System.err.println("Errore di rete: " + error);
    }

    public GameModel getGameModel() {
        return gameModel;
    }

    public NetworkClient getNetworkClient() {
        return networkClient;
    }
}
