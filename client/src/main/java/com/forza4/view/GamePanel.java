package com.forza4.view;

import javax.swing.JPanel;
import java.awt.Graphics;
import java.awt.Color;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import com.forza4.model.GameModel;
import com.forza4.controller.GameController;

// Pannello che disegna il board di Forza 4 (griglia 6x7)
public class GamePanel extends JPanel {
    private static final long serialVersionUID = 1L;
    
    private GameModel gameModel;
    private GameController controller;
    private final int CELL_SIZE = 60; // Dimensione di ogni cella in pixel
    
    // Costruttore
    public GamePanel(GameModel gameModel, GameController controller) {
        this.gameModel = gameModel;
        this.controller = controller;
        
        // Aggiunge listener per i click del mouse
        addMouseListener(new MouseAdapter() {
            @Override
            public void mousePressed(MouseEvent e) {
                handleColumnClick(e.getX());
            }
        });
    }
    
    // Gestisce il click su una colonna
    private void handleColumnClick(int x) {
        int col = x / CELL_SIZE;
        
        // Verifica che la colonna sia valida (0-6)
        if (col >= 0 && col < 7) {
            System.out.println("Colonna " + col + " cliccata");
            
            // Invia la mossa al controller
            if (gameModel.getCurrentMatchId() != null) {
                controller.makeMove(gameModel.getCurrentMatchId(), col);
            }
        }
    }
    
    // Disegna il board
    @Override
    protected void paintComponent(Graphics g) {
        super.paintComponent(g);
        
        int[][] board = gameModel.getBoard();
        
        // Disegna il background blu
        g.setColor(new Color(0, 102, 204)); // Blu
        g.fillRect(0, 0, getWidth(), getHeight());
        
        // Disegna le celle e i dischi
        for (int row = 0; row < 6; row++) {
            for (int col = 0; col < 7; col++) {
                int x = col * CELL_SIZE;
                int y = row * CELL_SIZE;
                
                // Disegna il bordo della cella
                g.setColor(Color.BLACK);
                g.drawRect(x, y, CELL_SIZE, CELL_SIZE);
                
                // Disegna il disco (se c'è)
                int cellValue = board[row][col];
                
                if (cellValue == 1) {
                    // Disco rosso (player 1)
                    g.setColor(Color.RED);
                    g.fillOval(x + 5, y + 5, CELL_SIZE - 10, CELL_SIZE - 10);
                } else if (cellValue == 2) {
                    // Disco giallo (player 2)
                    g.setColor(Color.YELLOW);
                    g.fillOval(x + 5, y + 5, CELL_SIZE - 10, CELL_SIZE - 10);
                }
                // Se cellValue == 0, la cella rimane vuota (blu)
            }
        }
        
        // Disegna il turno attuale
        g.setColor(Color.WHITE);
        g.setFont(g.getFont().deriveFont(16f));
        g.drawString("Turno: " + gameModel.getCurrentTurn(), 10, getHeight() - 10);
    }
}