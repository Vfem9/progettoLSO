 package com.forza4.model;

import java.util.ArrayList;
import java.util.List;
import java.util.Observer;

// Model che contiene lo stato del gioco
public class GameModel {
    private int[][] board; // Board 6x7
    private int currentTurn; // Turno attuale (1 o 2)
    private String currentMatchId; // ID della partita attuale
    private String playerUsername; // Username del giocatore
    private List<String> availableMatches; // Lista partite disponibili
    private List<Observer> observers; // Observer per la GUI
    
    // Costruttore
    public GameModel() {
        this.board = new int[6][7];
        this.currentTurn = 1;
        this.currentMatchId = null;
        this.playerUsername = null;
        this.availableMatches = new ArrayList<>();
        this.observers = new ArrayList<>();
        
        initBoard();
    }
    
    // Inizializza il board vuoto
    private void initBoard() {
        for (int row = 0; row < 6; row++) {
            for (int col = 0; col < 7; col++) {
                board[row][col] = 0;
            }
        }
    }
    
    // Aggiorna il model dai messaggi del server
    public void updateFromMessage(String message) {
        // TODO: Parsare il JSON e aggiornare il model
        System.out.println("Model aggiornato dal messaggio: " + message);
        notifyObservers();
    }
    
    // Aggiunge un observer (una view che ascolta i cambiamenti)
    public void addObserver(Observer observer) {
        observers.add(observer);
    }
    
    // Notifica tutti gli observer che il model è cambiato
    public void notifyObservers() {
        for (Observer observer : observers) {
            observer.update(null, null);
        }
    }
    
    // Getter e Setter
    public int[][] getBoard() {
        return board;
    }
    
    public void setBoard(int[][] board) {
        this.board = board;
        notifyObservers();
    }
    
    public int getCurrentTurn() {
        return currentTurn;
    }
    
    public void setCurrentTurn(int turn) {
        this.currentTurn = turn;
        notifyObservers();
    }
    
    public String getCurrentMatchId() {
        return currentMatchId;
    }
    
    public void setCurrentMatchId(String matchId) {
        this.currentMatchId = matchId;
        notifyObservers();
    }
    
    public String getPlayerUsername() {
        return playerUsername;
    }
    
    public void setPlayerUsername(String username) {
        this.playerUsername = username;
        notifyObservers();
    }
    
    public List<String> getAvailableMatches() {
        return availableMatches;
    }
    
    public void addAvailableMatch(String match) {
        availableMatches.add(match);
        notifyObservers();
    }
    
    public void clearAvailableMatches() {
        availableMatches.clear();
        notifyObservers();
    }
}
