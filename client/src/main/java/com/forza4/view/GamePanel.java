package com.forza4.view;

import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.SwingConstants;
import javax.swing.Timer;
import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Cursor;
import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.RenderingHints;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import com.forza4.model.GameModel;
import com.forza4.controller.GameController;

public class GamePanel extends JPanel {
    private static final long serialVersionUID = 1L;

    // Disegno e colori della board e dei dischi, coerenti con lo stile di Forza4.
    private static final Color BG_PAGE = new Color(245, 246, 248);
    private static final Color BOARD_BG = Color.WHITE;
    private static final Color BOARD_BORDER = new Color(210, 213, 219);
    private static final Color SLOT_EMPTY = new Color(226, 229, 234);
    private static final Color DISC_RED = new Color(224, 65, 62);
    private static final Color DISC_YELLOW = new Color(247, 197, 72);
    private static final Color ACCENT = new Color(41, 98, 255);

    private GameModel gameModel;
    private GameController controller;
    private final int CELL_SIZE = 64;
    private final int PADDING = 12;

    private JPanel boardArea;
    private JLabel statusLabel;

    // FIX: avviso temporaneo (es. "X si e' unito alla partita!") 
    private boolean showingBanner = false;
    private Timer bannerTimer;
    private static final Color BANNER_BG = new Color(56, 142, 60);

    public GamePanel(GameModel gameModel, GameController controller) {
        this.gameModel = gameModel;
        this.controller = controller;

        setLayout(new BorderLayout());
        setBackground(BG_PAGE);

        statusLabel = new JLabel(" ", SwingConstants.CENTER);
        statusLabel.setOpaque(true);
        statusLabel.setBackground(ACCENT);
        statusLabel.setForeground(Color.WHITE);
        statusLabel.setFont(statusLabel.getFont().deriveFont(Font.BOLD, 15f));
        statusLabel.setBorder(BorderFactory.createEmptyBorder(12, 8, 12, 8));
        add(statusLabel, BorderLayout.NORTH);

        boardArea = new JPanel() {
            private static final long serialVersionUID = 1L;
            @Override
            protected void paintComponent(Graphics g) {
                super.paintComponent(g);
                paintBoard((Graphics2D) g);
            }
        };
        boardArea.setPreferredSize(new Dimension(
            7 * CELL_SIZE + 2 * PADDING,
            6 * CELL_SIZE + 2 * PADDING));
        boardArea.setBackground(BOARD_BG);
        boardArea.setBorder(BorderFactory.createLineBorder(BOARD_BORDER, 1));
        boardArea.setCursor(Cursor.getPredefinedCursor(Cursor.HAND_CURSOR));
        boardArea.addMouseListener(new MouseAdapter() {
            @Override
            public void mousePressed(MouseEvent e) {
                handleColumnClick(e.getX() - PADDING);
            }
        });

        JPanel centerWrapper = new JPanel(new GridBagLayout());
        centerWrapper.setBackground(BG_PAGE);
        centerWrapper.add(boardArea, new GridBagConstraints());
        add(centerWrapper, BorderLayout.CENTER);

        JPanel bottomPanel = new JPanel(new FlowLayout(FlowLayout.CENTER, 8, 10));
        bottomPanel.setBackground(BG_PAGE);
        JButton lobbyButton = new JButton("Torna alla Lobby");
        lobbyButton.setFocusPainted(false);
        lobbyButton.setBorder(BorderFactory.createEmptyBorder(8, 16, 8, 16));
        // Gestisce l'evento di click sul pulsante "Torna alla Lobby" per lasciare la partita.
        lobbyButton.addActionListener(e -> {
            if (controller != null) {
                controller.leaveMatch();
            }
        });
        bottomPanel.add(lobbyButton);
        add(bottomPanel, BorderLayout.SOUTH);

        updateStatusLabel();
    }

    private void handleColumnClick(int x) {
        if (gameModel.getMatchState() != GameModel.STATE_ACTIVE) {
            System.out.println("La partita non e' attiva, click ignorato.");
            return;
        }

        int col = x / CELL_SIZE;

        if (col >= 0 && col < 7) {
            String matchId = gameModel.getCurrentMatchId();
            if (matchId != null && !matchId.isEmpty()) {
                controller.makeMove(matchId, col);
            }
        }
    }


    @Override
    public void repaint() {
        super.repaint();
        if (boardArea != null) {
            boardArea.repaint();
        }
        updateStatusLabel();
    }

    private void paintBoard(Graphics2D g) {
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);

        int[][] board = gameModel.getBoard();

        for (int row = 0; row < 6; row++) {
            for (int col = 0; col < 7; col++) {
                int x = PADDING + col * CELL_SIZE;
                int y = PADDING + row * CELL_SIZE;
                int d = CELL_SIZE - 14;

                int cellValue = board[row][col];
                if (cellValue == 1) {
                    g.setColor(DISC_RED);
                } else if (cellValue == 2) {
                    g.setColor(DISC_YELLOW);
                } else {
                    g.setColor(SLOT_EMPTY);
                }
                g.fillOval(x + 7, y + 7, d, d);
            }
        }
    }

    // Unione alla partita
    public void showTemporaryBanner(String text) {
        if (statusLabel == null) return;

        if (bannerTimer != null && bannerTimer.isRunning()) {
            bannerTimer.stop();
        }

        showingBanner = true;
        statusLabel.setBackground(BANNER_BG);
        statusLabel.setText(text);

        bannerTimer = new Timer(2000, e -> {
            showingBanner = false;
            statusLabel.setBackground(ACCENT);
            updateStatusLabel();
        });
        bannerTimer.setRepeats(false);
        bannerTimer.start();
    }

    private void updateStatusLabel() {
        if (statusLabel == null) return;

        if (showingBanner) return;

        int myNumber = gameModel.getMyPlayerNumber();
        String myColor = myNumber == 1 ? "Rosso" : myNumber == 2 ? "Giallo" : "?";

        String text;
        switch (gameModel.getMatchState()) {
            case GameModel.STATE_WAITING:
                text = "Sei " + myColor + " - in attesa dell'avversario...";
                break;
            case GameModel.STATE_ACTIVE:
                text = "Sei " + myColor + " - " + (gameModel.isMyTurn() ? "tocca a te!" : "turno dell'avversario...");
                break;
            case GameModel.STATE_FINISHED:
                if (gameModel.getWinnerId() == -1) {
                    text = "Partita terminata: pareggio.";
                } else {
                    text = gameModel.amIWinner() ? "Hai vinto!" : "Hai perso.";
                }
                break;
            default:
                text = " ";
        }
        statusLabel.setText(text);
    }
}
