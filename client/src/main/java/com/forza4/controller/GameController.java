package com.forza4.controller;

import com.forza4.model.GameModel;
import com.forza4.network.NetworkClient;
import com.forza4.view.LobbyPanel;

// Controller che orchestr i messaggi tra View, Model e Network
public class GameController implements NetworkClient.MessageListener {
    private NetworkClient networkClient;
    private GameModel gameModel;
    private LobbyPanel lobbyPanel;
    
    // Costruttore
    public GameController(LobbyPanel lobbyPanel) {
        this.lobbyPanel = lobbyPanel;
        this.gameModel = new GameModel();
        
        // Crea il client di rete
        this.networkClient = new NetworkClient("localhost", 5000, this);
    }
    
    // Connette al server
    public void connectToServer() {
        try {
            networkClient.connect();
            System.out.println("Connesso al server!");
        } catch (Exception e) {
            System.err.println("Errore connessione: " + e.getMessage());
        }
    }
    
    // Crea una nuova partita
    public void createMatch() {
        String message = "{\"type\":\"REQUEST\",\"action\":\"create_match\",\"msg_id\":\"msg_001\",\"payload\":{},\"error\":\"\"}";
        networkClient.sendMessage(message);
    }
    
    // Richiede la lista di partite disponibili
    public void listMatches() {
        String message = "{\"type\":\"REQUEST\",\"action\":\"list_matches\",\"msg_id\":\"msg_002\",\"payload\":{},\"error\":\"\"}";
        networkClient.sendMessage(message);
    }
    
    // Fa una mossa nel gioco
    public void makeMove(String matchId, int column) {
        String message = "{\"type\":\"REQUEST\",\"action\":\"move\",\"msg_id\":\"msg_003\",\"payload\":{\"match_id\":\"" + matchId + "\",\"column\":" + column + "},\"error\":\"\"}";
        networkClient.sendMessage(message);
    }
    
    // Callback quando arriva un messaggio dal server
    @Override
    public void onMessageReceived(String message) {
        System.out.println("Messaggio ricevuto dal server: " + message);
        
        // Parsa il JSON (da implementare correttamente dopo)
        gameModel.updateFromMessage(message);
        
        // Aggiorna la UI
        lobbyPanel.clearMatches();
        lobbyPanel.addMatch("Partita ricevuta dal server");
    }
    
    // Callback quando il server chiude la connessione
    @Override
    public void onConnectionClosed() {
        System.out.println("Disconnesso dal server");
    }
    
    // Callback quando c'è un errore di rete
    @Override
    public void onError(String error) {
        System.err.println("Errore di rete: " + error);
    }
    
    // Getter
    public GameModel getGameModel() {
        return gameModel;
    }
    
    public NetworkClient getNetworkClient() {
        return networkClient;
    }
}