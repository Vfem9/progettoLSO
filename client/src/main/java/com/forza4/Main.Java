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

            // Passa il MainFrame al controller PRIMA di tutto
            controller.setMainFrame(frame);

            // Passa il controller a MainFrame
            frame.setGameController(controller);

            // FIX: la richiesta dello username veniva mostrata PRIMA che la
            // finestra principale fosse visibile - il popup compariva quindi
            // "nel vuoto", senza contesto. Ora la finestra si apre per prima
            // e il popup viene mostrato subito dopo, agganciato ad essa.
            frame.setVisible(true);

            // FIX 2: chiamare askUsername() qui, nello STESSO blocco appena
            // dopo setVisible(true), non bastava. setVisible(true) mette in
            // coda gli eventi di "prima apparizione/disegno" della finestra
            // sull'Event Dispatch Thread, ma non li esegue subito: vengono
            // processati solo quando questo blocco di codice restituisce il
            // controllo alla coda eventi. Aprendo il JOptionPane modale
            // subito dopo, nello stesso blocco, il popup compariva PRIMA che
            // la finestra avesse fatto in tempo a disegnarsi davvero. Con un
            // secondo invokeLater annidato, la richiesta viene eseguita al
            // giro successivo della coda eventi: nel frattempo la finestra
            // ha gia' fatto in tempo a comparire e disegnarsi per intero.
            SwingUtilities.invokeLater(() -> {
                String username = askUsername(frame);
                controller.connectToServer(username);
            });
        });
    }

    // FIX: prima, se l'utente annullava o lasciava vuoto il campo, si
    // procedeva comunque con un nome generato in automatico. Ora la
    // richiesta si ripete finche' non viene inserito qualcosa di valido.
    private static String askUsername(java.awt.Component parent) {
        while (true) {
            String input = JOptionPane.showInputDialog(parent,
                "Inserisci il tuo username:", "Forza 4 - Accesso",
                JOptionPane.QUESTION_MESSAGE);

            if (input == null) {
                // L'utente ha annullato: ridomandiamo, non si entra senza nome.
                continue;
            }

            // Rimuove virgolette e backslash: il parser JSON minimale del
            // server (basato su sscanf, non un vero parser JSON) si rompe se
            // uno username contiene questi caratteri.
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
