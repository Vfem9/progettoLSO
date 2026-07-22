// Punto di ingresso dell'applicazione: inizializza finestra, controller e connessione al server.
package com.forza4;

import javax.swing.JOptionPane;
import javax.swing.SwingUtilities;
import com.forza4.view.MainFrame;
import com.forza4.controller.GameController;
import com.forza4.view.LobbyPanel;

public class Main {
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            LobbyPanel lobbyPanel = new LobbyPanel();
            MainFrame frame = new MainFrame(lobbyPanel);
            GameController controller = new GameController(lobbyPanel);

            // Passa il MainFrame al controller
            controller.setMainFrame(frame);

            // Passa il controller a MainFrame
            frame.setGameController(controller);

            // Richiesta username
            frame.setVisible(true);

            // Codice per impedire che la richiesta username appaia prima della crazione della finestra
            SwingUtilities.invokeLater(() -> {
                String username = askUsername(frame);
                controller.connectToServer(username);
            });
        });
    }

    // Loop finchè non viene immesso un username valido
    private static String askUsername(java.awt.Component parent) {
        while (true) {
            String input = JOptionPane.showInputDialog(parent,
                "Inserisci il tuo username:", "Forza 4 - Accesso",
                JOptionPane.QUESTION_MESSAGE);

            if (input == null) {
                continue;
            }

            // Rimuove virgolette e backslash: il parser JSON minimale del
            // server si rompe se un username contiene questi caratteri.
            String sanitized = input.trim().replaceAll("[\"\\\\]", "");

            if (!sanitized.isEmpty()) {
                return sanitized;
            }
            JOptionPane.showMessageDialog(parent,
                "Lo username non puo' essere vuoto. Riprova.",
                "Username non valido", JOptionPane.WARNING_MESSAGE);
        }
    }
}
