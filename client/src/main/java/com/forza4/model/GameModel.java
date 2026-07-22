package com.forza4.model;

// Model che contiene lo stato del gioco lato client.
public class GameModel {

    public static final int STATE_NONE = 0;      // nessuna partita
    public static final int STATE_WAITING = 1;   // creata, in attesa dell'avversario
    public static final int STATE_ACTIVE = 2;    // partita in corso
    public static final int STATE_FINISHED = 3;  // vittoria/pareggio/abbandono
    // Stato per chi ha richiesto di unirsi a una partita e sta aspettando la risposta del creatore.
    public static final int STATE_JOIN_PENDING = 4;

    private int[][] board;              // Board 6x7 (0 = vuoto, 1 = rosso, 2 = giallo)
    private int currentTurn;            // Simbolo (1 o 2) di chi deve muovere ora
    private String currentMatchId;      // ID della partita attuale
    private String playerUsername;      // Username scelto/generato per questo client

    private int myPlayerId = -1;        // id assegnato dal server alla connessione (register)
    private int myPlayerNumber = 0;     // 1 o 2: sono il creatore o chi si e' unito?
    private int opponentId = -1;
    private String opponentUsername;    // username scelto dall'avversario (server -> client)
    private int matchState = STATE_NONE;

    private int winnerId = -1;          // player_id del vincitore, -1 se nessuno/pareggio

    public GameModel() {
        this.board = new int[6][7];
        this.currentTurn = 1;
        this.currentMatchId = null;
        this.playerUsername = null;
    }

    private void initBoard() {
        board = new int[6][7];
    }

    public void resetForNewMatch() {
        initBoard();
        currentTurn = 1;
        matchState = STATE_WAITING;
        winnerId = -1;
    }

    public int[][] getBoard() {
        return board;
    }

    public void setBoard(int[][] board) {
        this.board = board;
    }

    public int getCurrentTurn() {
        return currentTurn;
    }

    public void setCurrentTurn(int turn) {
        this.currentTurn = turn;
    }

    public String getCurrentMatchId() {
        return currentMatchId;
    }

    public void setCurrentMatchId(String matchId) {
        this.currentMatchId = matchId;
    }

    public String getPlayerUsername() {
        return playerUsername;
    }

    public void setPlayerUsername(String username) {
        this.playerUsername = username;
    }

    public int getMyPlayerId() {
        return myPlayerId;
    }

    public void setMyPlayerId(int myPlayerId) {
        this.myPlayerId = myPlayerId;
    }

    public int getMyPlayerNumber() {
        return myPlayerNumber;
    }

    public void setMyPlayerNumber(int myPlayerNumber) {
        this.myPlayerNumber = myPlayerNumber;
    }

    public int getOpponentId() {
        return opponentId;
    }

    public void setOpponentId(int opponentId) {
        this.opponentId = opponentId;
    }

    public String getOpponentUsername() {
        return opponentUsername;
    }

    public void setOpponentUsername(String opponentUsername) {
        this.opponentUsername = opponentUsername;
    }

    public int getMatchState() {
        return matchState;
    }

    public void setMatchState(int matchState) {
        this.matchState = matchState;
    }

    public int getWinnerId() {
        return winnerId;
    }

    public void setWinnerId(int winnerId) {
        this.winnerId = winnerId;
    }

    // true se il colore di questo client corrisponde al turno attuale
    public boolean isMyTurn() {
        return matchState == STATE_ACTIVE && myPlayerNumber != 0 && myPlayerNumber == currentTurn;
    }

    public boolean amIWinner() {
        return winnerId != -1 && winnerId == myPlayerId;
    }
}
