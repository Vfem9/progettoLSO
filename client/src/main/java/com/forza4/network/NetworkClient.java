 package com.forza4.network;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;

// Client TCP per comunicare col server Forza 4
public class NetworkClient {
    private String host;
    private int port;
    private Socket socket;
    private PrintWriter output;
    private BufferedReader input;
    private Thread messageReaderThread;
    private MessageListener listener;
    private volatile boolean connected = false;
    
    // Interfaccia per ricevere i callback dai messaggi
    public interface MessageListener {
        void onMessageReceived(String message);
        void onConnectionClosed();
        void onError(String error);
    }
    
    // Costruttore
    public NetworkClient(String host, int port, MessageListener listener) {
        this.host = host;
        this.port = port;
        this.listener = listener;
    }
    
    // Connette al server
    public void connect() throws IOException {
        try {
            socket = new Socket(host, port);
            output = new PrintWriter(socket.getOutputStream(), true);
            input = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            connected = true;
            
            System.out.println("Connesso a " + host + ":" + port);
            
            // Avvia il thread che legge i messaggi dal server
            startMessageReaderThread();
            
        } catch (IOException e) {
            System.err.println("Errore di connessione: " + e.getMessage());
            throw e;
        }
    }
    
    // Avvia il thread che legge continuamente i messaggi dal server
    private void startMessageReaderThread() {
        messageReaderThread = new Thread(() -> {
            try {
                String line;
                // Rimane in loop finché il server non chiude la connessione
                while (connected && (line = input.readLine()) != null) {
                    System.out.println("Ricevuto dal server: " + line);
                    
                    // Chiama il callback con il messaggio ricevuto
                    if (listener != null) {
                        listener.onMessageReceived(line);
                    }
                }
                
                // Se arriviamo qui, il server ha chiuso la connessione
                if (connected) {
                    connected = false;
                    if (listener != null) {
                        listener.onConnectionClosed();
                    }
                }
                
            } catch (IOException e) {
                if (connected) {
                    System.err.println("Errore lettura messaggio: " + e.getMessage());
                    if (listener != null) {
                        listener.onError("Errore lettura: " + e.getMessage());
                    }
                }
            }
        });
        
        messageReaderThread.setDaemon(true);
        messageReaderThread.start();
    }
    
    // Invia un messaggio al server
    public void sendMessage(String message) {
        if (!connected) {
            System.err.println("Non connesso al server");
            return;
        }
        
        System.out.println("Inviato al server: " + message);
        output.println(message);
    }
    
    // Disconnette dal server
    public void disconnect() {
        if (!connected) return;
        
        connected = false;
        
        try {
            if (socket != null) socket.close();
            if (output != null) output.close();
            if (input != null) input.close();
            
            System.out.println("Disconnesso dal server");
            
        } catch (IOException e) {
            System.err.println("Errore disconnessione: " + e.getMessage());
        }
    }
    
    // Ritorna se è connesso
    public boolean isConnected() {
        return connected;
    }
}
