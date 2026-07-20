 package com.forza4.view;

import javax.swing.JFrame;
import javax.swing.JPanel;
import java.awt.BorderLayout;

// Finestra principale dell'applicazione
public class MainFrame extends JFrame {
    private static final long serialVersionUID = 1L;
    
    // Pannelli principali
    private LobbyPanel lobbyPanel;
    
    // Costruttore: inizializza la finestra
    public MainFrame() {
        setTitle("Forza 4 - Multi Client");
        setSize(800, 600);
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLocationRelativeTo(null); // Centra la finestra sullo schermo
        
        // Crea il pannello lobby (lista partite)
        lobbyPanel = new LobbyPanel();
        
        // Aggiunge il pannello alla finestra
        setContentPane(lobbyPanel);
    }
}
