package com.forza4.view;

import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.SwingConstants;
import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.FlowLayout;
import java.awt.Font;
import com.forza4.controller.GameController;

// FIX: nuovo pannello dedicato mostrato a chi ha appena richiesto di
// partecipare a una partita, mentre aspetta che il creatore accetti o
// rifiuti. Prima chi cliccava "Unisciti a Partita" entrava subito nella
// partita: ora deve prima aspettare una decisione, e questa schermata
// (con un bottone "Annulla richiesta") gli da' un feedback chiaro invece
// di lasciarlo a fissare la lobby senza sapere cosa sta succedendo.
public class JoinWaitingPanel extends JPanel {
    private static final long serialVersionUID = 1L;

    private static final Color BG_PAGE = new Color(245, 246, 248);
    private static final Color TEXT_DARK = new Color(33, 37, 41);
    private static final Color TEXT_MUTED = new Color(120, 126, 133);

    private GameController controller;
    private final JLabel messageLabel;

    public JoinWaitingPanel(GameController controller) {
        this.controller = controller;

        setLayout(new BorderLayout());
        setBackground(BG_PAGE);
        setBorder(BorderFactory.createEmptyBorder(16, 16, 16, 16));

        messageLabel = new JLabel("In attesa di una risposta...", SwingConstants.CENTER);
        messageLabel.setFont(messageLabel.getFont().deriveFont(Font.PLAIN, 16f));
        messageLabel.setForeground(TEXT_DARK);
        add(messageLabel, BorderLayout.CENTER);

        JLabel hint = new JLabel("Puoi annullare la richiesta in qualsiasi momento.", SwingConstants.CENTER);
        hint.setFont(hint.getFont().deriveFont(Font.PLAIN, 12f));
        hint.setForeground(TEXT_MUTED);
        hint.setBorder(BorderFactory.createEmptyBorder(8, 0, 0, 0));
        add(hint, BorderLayout.NORTH);

        JPanel bottomPanel = new JPanel(new FlowLayout(FlowLayout.CENTER, 8, 16));
        bottomPanel.setBackground(BG_PAGE);
        JButton cancelButton = new JButton("Annulla richiesta");
        cancelButton.setFocusPainted(false);
        cancelButton.setBorder(BorderFactory.createEmptyBorder(8, 16, 8, 16));
        cancelButton.addActionListener(e -> {
            if (this.controller != null) {
                this.controller.cancelJoinRequest();
            }
        });
        bottomPanel.add(cancelButton);
        add(bottomPanel, BorderLayout.SOUTH);
    }

    public void setWaitingText(String text) {
        messageLabel.setText(text);
    }
}
