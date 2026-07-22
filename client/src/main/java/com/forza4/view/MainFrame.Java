package com.forza4.view;

import javax.swing.JFrame;
import com.forza4.model.GameModel;
import com.forza4.controller.GameController;

public class MainFrame extends JFrame {
    private static final long serialVersionUID = 1L;

    private LobbyPanel lobbyPanel;
    private GamePanel gamePanel;
    private JoinWaitingPanel joinWaitingPanel;
    private GameModel gameModel;
    private GameController gameController;

    public MainFrame(LobbyPanel lobbyPanel) {
        this.lobbyPanel = lobbyPanel;
        this.gameModel = new GameModel();

        setTitle("Forza 4 - Multi Client");
        setSize(820, 660);
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLocationRelativeTo(null);

        setContentPane(lobbyPanel);
    }

    public void setGameController(GameController controller) {
        this.gameController = controller;
        this.gameModel = controller.getGameModel();
    }

    public void switchToGamePanel() {
        gamePanel = new GamePanel(gameModel, gameController);
        setContentPane(gamePanel);
        gamePanel.revalidate();
        gamePanel.repaint();
        System.out.println("Passato al GamePanel!");
    }

    public void switchToLobbyPanel() {
        setContentPane(lobbyPanel);
        lobbyPanel.revalidate();
        lobbyPanel.repaint();
        System.out.println("Tornato al LobbyPanel!");
    }

    // Schermata mostrata a chi ha richiesto di partecipare a una
    // partita, in attesa che il creatore accetti o rifiuti.
    public void switchToJoinWaitingPanel() {
        joinWaitingPanel = new JoinWaitingPanel(gameController);
        setContentPane(joinWaitingPanel);
        joinWaitingPanel.revalidate();
        joinWaitingPanel.repaint();
        System.out.println("Passato alla schermata di attesa richiesta!");
    }

    public LobbyPanel getLobbyPanel() {
        return lobbyPanel;
    }

    public GamePanel getGamePanel() {
        return gamePanel;
    }

    public JoinWaitingPanel getJoinWaitingPanel() {
        return joinWaitingPanel;
    }
}
