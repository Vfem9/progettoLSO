package com.forza4.network;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public class NetworkClient {
    private String host;
    private int port;
    private Socket socket;
    private OutputStream output;
    private BufferedReader input;
    private Thread messageReaderThread;
    private MessageListener listener;
    private volatile boolean connected = false;

    public interface MessageListener {
        void onMessageReceived(String message);
        void onConnectionClosed();
        void onError(String error);
    }

    public NetworkClient(String host, int port, MessageListener listener) {
        this.host = host;
        this.port = port;
        this.listener = listener;
    }

    public void connect() throws IOException {
        try {
            socket = new Socket(host, port);
            output = socket.getOutputStream();
            input = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
            connected = true;

            System.out.println("Connesso a " + host + ":" + port);

            startMessageReaderThread();

        } catch (IOException e) {
            System.err.println("Errore di connessione: " + e.getMessage());
            throw e;
        }
    }

    // FIX: prima si faceva polling ogni 50ms su input.available(), il che
    // aggiungeva fino a 50ms di latenza per ogni messaggio (percepibile su una
    // board che deve aggiornarsi "in tempo reale" ad ogni mossa) oltre a
    // sprecare CPU. readLine() su un BufferedReader si blocca finche' non
    // arriva una riga completa (i messaggi sono terminati da \n lato server),
    // quindi e' sia piu' reattivo che piu' semplice.
    private void startMessageReaderThread() {
        messageReaderThread = new Thread(() -> {
            try {
                String line;
                while (connected && (line = input.readLine()) != null) {
                    String message = line.trim();
                    if (!message.isEmpty()) {
                        System.out.println("Ricevuto dal server: " + message);
                        if (listener != null) {
                            listener.onMessageReceived(message);
                        }
                    }
                }
                // readLine() ha ritornato null: il server ha chiuso la connessione
                if (connected && listener != null) {
                    listener.onConnectionClosed();
                }
            } catch (IOException e) {
                if (connected) {
                    System.err.println("Errore lettura messaggio: " + e.getMessage());
                    if (listener != null) {
                        listener.onError("Errore lettura: " + e.getMessage());
                    }
                }
            } finally {
                connected = false;
            }
        });

        messageReaderThread.setDaemon(true);
        messageReaderThread.start();
    }

    public synchronized void sendMessage(String message) {
        if (!connected) {
            System.err.println("Non connesso al server");
            return;
        }

        try {
            String msg = message + "\n";
            output.write(msg.getBytes(StandardCharsets.UTF_8));
            output.flush();
            System.out.println("Inviato al server: " + message);
        } catch (IOException e) {
            System.err.println("Errore invio: " + e.getMessage());
        }
    }

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

    public boolean isConnected() {
        return connected;
    }
}
