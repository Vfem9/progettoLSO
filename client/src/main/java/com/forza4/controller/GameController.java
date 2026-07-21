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
    // chi ha perso. Lo teniamo qui cosi', quando arriva la vera decisione
    // dell'avversario (rivincita o abbandono), possiamo chiuderlo noi stessi
    // invece di lasciare che si accumuli un secondo dialog sopra.
    private JDialog waitingDialog;

    // FIX: popup "il creatore sta valutando un'altra richiesta" mostrato a
    // chi finisce in coda. Prima non veniva mai chiuso in automatico: se nel
    // frattempo la richiesta davanti in coda si liberava (rifiutata o
    // annullata) e la propria veniva promossa, questo popup restava aperto
    // per sempre (bug segnalato dall'utente). Tracciato qui per poterlo
    // chiudere programmaticamente non appena arriva ACTION_JOIN_REQUEST_PROMOTED.
    private JDialog queuedInfoDialog;

    public GameController(LobbyPanel lobbyPanel) {
        this.lobbyPanel = lobbyPanel;
        this.gameModel = new GameModel();

        this.networkClient = new NetworkClient("localhost", 5000, this);

        this.lobbyPanel.setController(this);
    }

    public void setMainFrame(MainFrame mainFrame) {
        this.mainFrame = mainFrame;
    }

    // FIX: prima il client non si registrava mai (Main.java mandava solo un
    // "ping" di debug che il server non gestisce), oppure si registrava con
    // uno username generato in automatico senza mai chiedere nulla
    // all'utente. Ora l'username arriva da chi chiama (Main.java lo chiede
    // con un dialog all'avvio) e viene usato per la registrazione reale.
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
        // FIX: prima non veniva controllato affatto se fosse il proprio turno,
        // il client mandava la mossa e basta lasciando fare tutto il lavoro di
        // validazione al server (che comunque la rifiuta, ma senza dare un
        // buon feedback prima di questa serie di fix). Filtriamo qui per una
        // UX piu' pulita; il server resta comunque l'autorita' finale.
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
        // FIX: il parsing arrivava direttamente sul thread di lettura della rete
        // (NetworkClient.messageReaderThread) e da li' venivano chiamati
        // direttamente metodi Swing (repaint, switchToGamePanel, aggiornamento
        // liste...) violando la regola "un solo thread tocca la UI" di Swing.
        // Poteva causare glitch grafici intermittenti, particolarmente fastidiosi
        // per una board che deve aggiornarsi ad ogni mossa. Ora tutto il
        // trattamento del messaggio (parsing incluso, per semplicita') gira
        // sull'Event Dispatch Thread.
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

        // FIX: prima il campo "error" della risposta non veniva mai controllato.
        // Se il server rispondeva con "Not your turn", "Match not found" ecc.
        // l'utente non vedeva assolutamente nulla.
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
            // FIX: prima si poteva creare/unirsi a una partita anche prima
            // che la registrazione fosse confermata dal server (i bottoni
            // erano sempre attivi). Ora la lobby li tiene disabilitati finche'
            // non arriva questa risposta.
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
                    // FIX: prima veniva mostrato solo l'id numerico del creatore
                    // ("Creator: 3"), poco leggibile. Il server ora manda anche
                    // "creator_username" (il nome scelto da chi ha creato la
                    // partita), che usiamo qui al posto del numero.
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
            // FIX: prima questa risposta significava "sei entrato subito in
            // partita". Ora e' solo la conferma che la RICHIESTA e' stata
            // ricevuta: puo' essere "pending" (il creatore la sta valutando
            // adesso) o "queued" (il creatore sta gia' valutando qualcun
            // altro, questa e' in coda). In entrambi i casi si passa a una
            // schermata di attesa dedicata con un bottone per annullare; il
            // vero ingresso in partita arriva poi come NOTIFICATION
            // ACTION_MATCH_STARTED, se/quando il creatore accetta.
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
                // FIX: prima era un JOptionPane.showMessageDialog statico,
                // impossibile da chiudere via codice. Ora e' un JDialog vero
                // e proprio, tracciato in `queuedInfoDialog`, cosi' possiamo
                // disporlo noi stessi non appena arriva la promozione dalla
                // coda (o comunque la richiesta smette di essere valida),
                // invece di lasciarlo aperto per sempre.
                JOptionPane pane = new JOptionPane(
                    creatorUsername + " sta valutando un'altra richiesta.\nLa tua richiesta e' in coda.",
                    JOptionPane.INFORMATION_MESSAGE);
                queuedInfoDialog = pane.createDialog(mainFrame, "Richiesta in coda");
                queuedInfoDialog.setVisible(true); // si sblocca anche da solo, vedi closeQueuedInfoDialogIfOpen()
                queuedInfoDialog = null;
            }
        }
        // ACTION_MOVE in RESPONSE arriva solo in caso di errore (gia' gestito
        // sopra da handleError), quindi qui non c'e' altro da fare.
        else if (MessageProtocol.ACTION_REMATCH.equals(action)) {
            // FIX (fedelta' alla traccia): prima la risposta era un "ok"
            // muto, lo stato vero arrivava sempre e solo con la NOTIFICATION
            // match_started poco dopo. Ora la risposta puo' portare essa
            // stessa lo stato in due casi nuovi:
            // - "new_session": sono il vincitore e ho aperto una nuova
            //   sessione di cui sono proprietario - mi comporto come se
            //   avessi appena creato una partita da zero (torno in stato di
            //   attesa, board vuota, nessun avversario ancora assegnato).
            // - "waiting_for_opponent_vote": e' un pareggio, ho votato "si"
            //   ma l'altro giocatore non ha ancora deciso - resto in attesa,
            //   nessun cambio di stato locale.
            // Il caso "ok" semplice (pareggio, entrambi hanno votato si) non
            // richiede nulla qui: lo stato vero arriva con la NOTIFICATION
            // match_started, esattamente come prima.
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
            // rivincita, chiudiamolo: la nuova board che sta per arrivare e'
            // di per se' la risposta.
            closeWaitingDialogIfOpen();
            closeQueuedInfoDialogIfOpen();

            // FIX: bisogna distinguere TRE casi possibili, leggendo lo stato
            // PRIMA di sovrascriverlo con applyMatchState:
            // - STATE_WAITING: ero il creatore in attesa, il creatore ora ha
            //   accettato una richiesta -> avviso "X si e' unito alla partita!"
            // - STATE_JOIN_PENDING: avevo richiesto IO di partecipare ed e'
            //   arrivata l'accettazione -> avviso "Ti sei unito alla partita
            //   di X!"
            // - STATE_FINISHED: e' una rivincita -> nessun avviso, si rientra
            //   semplicemente in partita.
            boolean wasCreatorWaiting = gameModel.getMatchState() == GameModel.STATE_WAITING;
            boolean wasJoinPending = gameModel.getMatchState() == GameModel.STATE_JOIN_PENDING;

            // FIX: prima il creatore della partita non veniva MAI avvisato che
            // l'avversario si era unito: restava bloccato a guardare una board
            // vuota senza sapere che poteva giocare. Questa notifica (mandata
            // anche dopo una rivincita) sblocca la board per entrambi.
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
            // FIX: nuovo passaggio richiesto dall'utente - il creatore non
            // accetta piu' automaticamente chi si unisce, deve confermarlo
            // esplicitamente con un popup si/no.
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
            // FIX: chi aveva richiesto di partecipare ora viene avvisato se
            // il creatore rifiuta, invece di restare bloccato per sempre
            // nella schermata di attesa.
            closeQueuedInfoDialogIfOpen();
            String creatorUsername = payload.has("creator_username") ? payload.get("creator_username").getAsString() : "Il creatore";
            JOptionPane.showMessageDialog(mainFrame,
                creatorUsername + " ha rifiutato la tua richiesta di partecipare.",
                "Richiesta rifiutata", JOptionPane.INFORMATION_MESSAGE);
            returnToLobby();
        }
        else if (MessageProtocol.ACTION_JOIN_REQUEST_OBSOLETE.equals(action)) {
            // Chi era in coda (o in decisione) quando la partita e' stata
            // chiusa dal creatore, oppure e' iniziata con qualcun altro.
            closeQueuedInfoDialogIfOpen();
            String reason = payload.has("reason") ? payload.get("reason").getAsString() : "closed";
            String message = "started".equals(reason)
                ? "La partita a cui volevi partecipare e' gia' iniziata con un altro giocatore."
                : "La partita non e' piu' disponibile.";
            JOptionPane.showMessageDialog(mainFrame, message, "Partita non disponibile", JOptionPane.INFORMATION_MESSAGE);
            returnToLobby();
        }
        else if (MessageProtocol.ACTION_MATCH_UNAVAILABLE.equals(action)) {
            // FIX (fedelta' alla traccia): prima gli altri client scoprivano
            // che una partita non era piu' disponibile solo al refresh
            // periodico della lobby (fino a 3s dopo). Ora il server manda
            // subito questo avviso broadcast a tutti tranne i due
            // partecipanti: aggiorniamo la lista immediatamente. Se non
            // siamo in lobby in questo momento (es. si e' gia' in un'altra
            // partita) la chiamata e' comunque innocua.
            listMatches();
        }
        else if (MessageProtocol.ACTION_JOIN_REQUEST_PROMOTED.equals(action)) {
            // FIX: bug segnalato dall'utente - chi era in coda vedeva il
            // popup "il creatore sta valutando un'altra richiesta" restare
            // aperto per sempre anche dopo essere stato promosso (la
            // richiesta davanti e' stata rifiutata/annullata). Ora lo
            // chiudiamo noi stessi non appena arriva questa notifica: non
            // serve altro, il client resta gia' nella schermata di attesa
            // (JoinWaitingPanel) in entrambi i casi (in coda o in decisione).
            closeQueuedInfoDialogIfOpen();
        }
        else if (MessageProtocol.ACTION_MOVE.equals(action)) {
            // FIX principale lato client: prima la mossa non veniva mai scritta
            // nel model (ne' per chi giocava ne' tantomeno per l'avversario, che
            // non riceveva proprio nulla dal server). Ora scriviamo la board
            // ricevuta e ridisegniamo.
            int[][] board = parseBoard(payload);
            gameModel.setBoard(board);

            if (payload.has("current_turn")) {
                gameModel.setCurrentTurn(payload.get("current_turn").getAsInt());
            }

            // FIX: repaint() veniva chiamato QUI, prima di scrivere nel model
            // l'esito della partita (matchState/winnerId) qualche riga sotto.
            // Per chi vinceva o pareggiava non si notava (ci pensava comunque
            // il JOptionPane a dare il verdetto), ma per chi perdeva - che dal
            // fix precedente non ha piu' nessun popup, solo l'etichetta di
            // stato nel GamePanel - il repaint ridisegnava l'etichetta CON LO
            // STATO VECCHIO (partita ancora attiva), perche' matchState/
            // winnerId non erano ancora stati aggiornati. Risultato: "Hai
            // perso" non compariva mai. Ora aggiorniamo prima tutto lo stato
            // e solo alla fine chiediamo un unico repaint.
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
            // FIX: prima una disconnessione dell'avversario lasciava l'altro
            // giocatore in attesa per sempre, senza nessun avviso. Lo stesso
            // valeva se l'avversario cliccava "Torna alla Lobby" a meta'
            // partita invece di disconnettersi davvero - il server distingue
            // i due casi col campo "reason" cosi' mostriamo il messaggio giusto.
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
            // FIX: quando il vincitore rifiutava la rivincita (o comunque
            // l'avversario lasciava una partita gia' finita), l'altro
            // giocatore non veniva mai avvisato e restava bloccato a
            // guardare "Hai perso"/"Hai vinto" per sempre. Chiudiamo prima
            // l'eventuale popup "in attesa dell'avversario" (se e' ancora
            // aperto, evitando che si accumuli sopra questo) e mostriamo
            // l'unico dialog reale con l'esito.
            closeWaitingDialogIfOpen();
            if (gameModel.getMatchState() == GameModel.STATE_FINISHED) {
                JOptionPane.showMessageDialog(mainFrame,
                    "L'avversario ha lasciato la partita.",
                    "Partita chiusa", JOptionPane.INFORMATION_MESSAGE);
                returnToLobby();
            }
        }
    }

    // Chiude il popup "in attesa dell'avversario" se e' ancora aperto (chi ha
    // perso lo sta ancora guardando quando arriva la vera decisione
    // dell'avversario). dispose() su un JDialog modale ancora visibile fa
    // ritornare la chiamata bloccante che lo aveva aperto (setVisible(true)),
    // quindi non serve altro per "sbloccarlo".
    private void closeWaitingDialogIfOpen() {
        if (waitingDialog != null && waitingDialog.isShowing()) {
            waitingDialog.dispose();
        }
        waitingDialog = null;
    }

    // Stesso meccanismo di closeWaitingDialogIfOpen(), ma per il popup "in
    // coda" mostrato a chi ha richiesto di partecipare mentre il creatore
    // stava gia' valutando qualcun altro.
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
        // FIX: il server ora manda anche "opponent_username" (il nome scelto
        // dall'altro giocatore) in create_match/join_match/match_started;
        // lo salviamo nel model per poterlo mostrare negli avvisi in GamePanel.
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

    // Dialog di fine partita.
    //
    // FIX (fedelta' alla traccia): prima QUALSIASI dei due giocatori poteva
    // far ripartire la partita da solo (bastava un "Si", l'altro veniva
    // riunito automaticamente). La traccia invece distingue due casi:
    // - Vittoria/sconfitta: "il perdente e' obbligato a lasciare la
    //   partita" - niente scelta, solo un avviso, poi si esce subito. "Il
    //   vincitore... puo' decidere di creare una nuova sessione,
    //   diventando automaticamente il nuovo proprietario" - la nuova
    //   sessione torna in stato di attesa ed e' aperta a chiunque in lobby,
    //   non necessariamente allo stesso avversario di prima.
    // - Pareggio: "entrambi i giocatori hanno la possibilita' di scegliere
    //   CONGIUNTAMENTE se effettuare una nuova partita" - serve il consenso
    //   di entrambi, non basta che uno solo lo richieda.
    private void showGameOverDialog(boolean isDraw) {
        boolean iWon = gameModel.amIWinner();

        if (isDraw) {
            int choice = JOptionPane.showConfirmDialog(mainFrame,
                "Pareggio!\nVuoi proporre una nuova partita?\n(serve che anche l'avversario sia d'accordo)",
                "Partita terminata", JOptionPane.YES_NO_OPTION);
            if (choice == JOptionPane.YES_OPTION) {
                requestRematch();
            } else {
                // Avvisiamo il server (che a sua volta avvisa l'eventuale
                // avversario in attesa via ACTION_MATCH_CLOSED).
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
            // FIX: prima chi perdeva vedeva un popup "in attesa che
            // l'avversario decida" e restava bloccato finche' il vincitore
            // non sceglieva. La traccia impone invece che il perdente sia
            // OBBLIGATO a lasciare la partita, senza alcuna scelta ne'
            // attesa: la decisione del vincitore (aprire una nuova sessione
            // o no) e' del tutto indipendente e non lo riguarda piu'.
            JOptionPane.showMessageDialog(mainFrame,
                "Hai perso.", "Partita terminata", JOptionPane.INFORMATION_MESSAGE);
            leaveMatch();
        }
    }

    // FIX: prima GamePanel chiamava direttamente returnToLobby() per il
    // bottone "Torna alla Lobby", senza mai avvisare il server - se usato a
    // meta' partita, l'avversario restava a fissare la board per sempre. Ora
    // questo metodo e' pubblico e usato sia da qui (rifiuto rivincita) sia
    // dal bottone di GamePanel: il server (ACTION_QUIT_MATCH) decide da solo
    // il da farsi in base allo stato della partita (abbandono se attiva,
    // semplice chiusura se gia' finita, nessun avviso se non c'era ancora
    // un avversario).
    public void leaveMatch() {
        String matchId = gameModel.getCurrentMatchId();
        if (matchId != null) {
            JsonObject payload = new JsonObject();
            payload.addProperty("match_id", matchId);
            networkClient.sendMessage(MessageProtocol.createRequestMessage(MessageProtocol.ACTION_QUIT_MATCH, payload));
        }
        returnToLobby();
    }

    // FIX: nuovo metodo per il bottone "Annulla richiesta" della schermata
    // di attesa (JoinWaitingPanel). Avvisa il server che la richiesta di
    // partecipazione (in decisione o ancora in coda) non interessa piu',
    // cosi' il creatore non resta a valutare una richiesta di qualcuno che
    // se n'e' gia' andato.
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
