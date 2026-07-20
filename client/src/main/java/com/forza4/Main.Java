package com.forza4;

import javax.swing.SwingUtilities;
import com.forza4.view.MainFrame;

// Entry point dell'applicazione: punto di inizio del programma
public class Main {
    public static void main(String[] args) {
        // SwingUtilities.invokeLater: Esegue il codice nel thread di Swing (thread-safe)
        SwingUtilities.invokeLater(() -> {
            MainFrame frame = new MainFrame();
            frame.setVisible(true);
        });
    }
}