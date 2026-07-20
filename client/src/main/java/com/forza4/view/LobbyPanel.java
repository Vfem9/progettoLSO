package com.forza4.view;

import javax.swing.JPanel;
import javax.swing.JButton;
import javax.swing.JList;
import javax.swing.JScrollPane;
import javax.swing.DefaultListModel;
import javax.swing.JLabel;
import java.awt.BorderLayout;
import java.awt.FlowLayout;

// Pannello della lobby: mostra la lista di partite e bottoni per creare/unirsi
public class LobbyPanel extends JPanel {
    private static final long serialVersionUID = 1L;
    
    // Componenti GUI
    private JButton createMatchButton;
    private JButton joinMatchButton;
    private JButton refreshButton;
    private JList<String> matchesList;
    private DefaultListModel<String> matchesListModel;
    
    // Costruttore: inizializza il pannello
    public LobbyPanel() {
        setLayout(new BorderLayout());
        
        // Pannello superiore con bottoni
        JPanel topPanel = new JPanel(new FlowLayout());
        
        createMatchButton = new JButton("Crea Partita");
        joinMatchButton = new JButton("Unisciti a Partita");
        refreshButton = new JButton("Ricarica");
        
        topPanel.add(new JLabel("Partite Disponibili:"));
        topPanel.add(createMatchButton);
        topPanel.add(joinMatchButton);
        topPanel.add(refreshButton);
        
        // Pannello centrale con lista di partite
        matchesListModel = new DefaultListModel<>();
        matchesList = new JList<>(matchesListModel);
        JScrollPane scrollPane = new JScrollPane(matchesList);
        
        // Aggiunge i pannelli
        add(topPanel, BorderLayout.NORTH);
        add(scrollPane, BorderLayout.CENTER);
        
        // Aggiunge listener ai bottoni (da implementare dopo)
        addButtonListeners();
    }
    
    // Aggiunge i listener ai bottoni
    private void addButtonListeners() {
        createMatchButton.addActionListener(e -> onCreateMatchClicked());
        joinMatchButton.addActionListener(e -> onJoinMatchClicked());
        refreshButton.addActionListener(e -> onRefreshClicked());
    }
    
    // Callback quando si clicca "Crea Partita"
    private void onCreateMatchClicked() {
        System.out.println("Bottone 'Crea Partita' cliccato");
        // Da implementare: comunicare col server
    }
    
    // Callback quando si clicca "Unisciti a Partita"
    private void onJoinMatchClicked() {
        System.out.println("Bottone 'Unisciti a Partita' cliccato");
        // Da implementare: comunicare col server
    }
    
    // Callback quando si clicca "Ricarica"
    private void onRefreshClicked() {
        System.out.println("Bottone 'Ricarica' cliccato");
        // Da implementare: chiedere al server la lista aggiornata
    }
    
    // Metodi pubblici per aggiornare la lista di partite
    public void addMatch(String matchInfo) {
        matchesListModel.addElement(matchInfo);
    }
    
    public void clearMatches() {
        matchesListModel.clear();
    }
    
    public String getSelectedMatch() {
        return matchesList.getSelectedValue();
    }
}