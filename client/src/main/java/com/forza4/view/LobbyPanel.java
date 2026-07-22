package com.forza4.view;

import javax.swing.BorderFactory;
import javax.swing.BoxLayout;
import javax.swing.DefaultListModel;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.ListCellRenderer;
import javax.swing.ListSelectionModel;
import javax.swing.SwingConstants;
import javax.swing.Timer;
import javax.swing.border.EmptyBorder;
import java.awt.BorderLayout;
import java.awt.CardLayout;
import java.awt.Color;
import java.awt.Component;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.event.HierarchyEvent;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import com.forza4.controller.GameController;

// Pannello della lobby: mostra la lista di partite e bottoni per creare/unirsi
public class LobbyPanel extends JPanel {
    private static final long serialVersionUID = 1L;

    private static final Color BG_PAGE = new Color(245, 246, 248);
    private static final Color ACCENT = new Color(41, 98, 255);
    private static final Color TEXT_DARK = new Color(33, 37, 41);
    private static final Color TEXT_MUTED = new Color(120, 126, 133);
    private static final Color PILL_WAITING = new Color(255, 152, 0);
    // Le partite in corso sono visibili ma non unibili: badge grigio scuro.
    private static final Color PILL_ACTIVE = new Color(117, 117, 117);
    private static final Color BORDER_LIGHT = new Color(210, 213, 219);

    // Info partita: "<match_id> (Creator: <username>, Status: <status>)"
    private static final Pattern MATCH_INFO_PATTERN =
        Pattern.compile("^(.+) \\(Creator: ([^,]+), Status: (\\w+)\\)$");

    private static final String CARD_EMPTY = "empty";
    private static final String CARD_LIST = "list";

    private JButton createMatchButton;
    private JButton joinMatchButton;
    private JButton refreshButton;
    private JList<String> matchesList;
    private DefaultListModel<String> matchesListModel;

    private CardLayout listCardLayout;
    private JPanel listCardsPanel;

    // Aggiorna la lista delle lobby.
    private final Timer autoRefreshTimer;

    private GameController controller;

    public LobbyPanel() {
        setLayout(new BorderLayout());
        setBackground(BG_PAGE);
        setBorder(new EmptyBorder(16, 16, 16, 16));

        JLabel title = new JLabel("Forza 4");
        title.setFont(title.getFont().deriveFont(Font.BOLD, 26f));
        title.setForeground(TEXT_DARK);
        title.setBorder(new EmptyBorder(0, 2, 12, 0));

        JLabel subtitle = new JLabel("Partite disponibili");
        subtitle.setFont(subtitle.getFont().deriveFont(Font.PLAIN, 14f));
        subtitle.setForeground(TEXT_MUTED);

        JPanel headerText = new JPanel();
        headerText.setLayout(new BoxLayout(headerText, BoxLayout.Y_AXIS));
        headerText.setBackground(BG_PAGE);
        headerText.add(title);
        headerText.add(subtitle);

        createMatchButton = styledButton("+  Crea Partita", ACCENT, Color.WHITE);
        joinMatchButton = styledButton("→  Unisciti a Partita", Color.WHITE, ACCENT);
        refreshButton = styledButton("⟳  Ricarica", Color.WHITE, TEXT_DARK);

        // Controlla che l'utente sia registrato prima di unire/creare partite.
        createMatchButton.setEnabled(false);
        joinMatchButton.setEnabled(false);

        JPanel buttonsPanel = new JPanel(new FlowLayout(FlowLayout.LEFT, 8, 0));
        buttonsPanel.setBackground(BG_PAGE);
        buttonsPanel.add(createMatchButton);
        buttonsPanel.add(joinMatchButton);
        buttonsPanel.add(refreshButton);

        JPanel topPanel = new JPanel(new BorderLayout());
        topPanel.setBackground(BG_PAGE);
        topPanel.add(headerText, BorderLayout.NORTH);
        topPanel.add(buttonsPanel, BorderLayout.SOUTH);
        topPanel.setBorder(new EmptyBorder(0, 0, 14, 0));

        matchesListModel = new DefaultListModel<>();
        matchesList = new JList<>(matchesListModel);
        matchesList.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        matchesList.setBackground(Color.WHITE);
        matchesList.setFixedCellHeight(56);
        matchesList.setCellRenderer(new MatchCellRenderer());
        matchesList.setBorder(new EmptyBorder(4, 4, 4, 4));

        JScrollPane scrollPane = new JScrollPane(matchesList);
        scrollPane.setBorder(BorderFactory.createLineBorder(BORDER_LIGHT, 1));

        JPanel emptyStatePanel = buildEmptyStatePanel();

        listCardLayout = new CardLayout();
        listCardsPanel = new JPanel(listCardLayout);
        listCardsPanel.add(emptyStatePanel, CARD_EMPTY);
        listCardsPanel.add(scrollPane, CARD_LIST);
        listCardLayout.show(listCardsPanel, CARD_EMPTY);

        add(topPanel, BorderLayout.NORTH);
        add(listCardsPanel, BorderLayout.CENTER);

        addButtonListeners();

        autoRefreshTimer = new Timer(3000, e -> {
            if (controller != null) {
                controller.listMatches();
            }
        });
        addHierarchyListener(e -> {
            if ((e.getChangeFlags() & HierarchyEvent.SHOWING_CHANGED) != 0) {
                if (isShowing()) {
                    autoRefreshTimer.start();
                } else {
                    autoRefreshTimer.stop();
                }
            }
        });
    }

    private JPanel buildEmptyStatePanel() {
        JPanel panel = new JPanel(new java.awt.GridBagLayout());
        panel.setBackground(Color.WHITE);
        panel.setBorder(BorderFactory.createLineBorder(BORDER_LIGHT, 1));

        JLabel label = new JLabel("<html><div style='text-align:center;'>"
            + "Nessuna partita disponibile al momento.<br>"
            + "Creane una tu per iniziare a giocare!</div></html>", SwingConstants.CENTER);
        label.setFont(label.getFont().deriveFont(Font.PLAIN, 14f));
        label.setForeground(TEXT_MUTED);
        panel.add(label);
        return panel;
    }

    private JButton styledButton(String text, Color bg, Color fg) {
        JButton button = new JButton(text);
        button.setFocusPainted(false);
        button.setOpaque(true);
        button.setContentAreaFilled(true);
        button.setBackground(bg);
        button.setForeground(fg);
        button.setFont(button.getFont().deriveFont(Font.BOLD, 13f));
        button.setBorder(BorderFactory.createCompoundBorder(
            BorderFactory.createLineBorder(bg == Color.WHITE ? BORDER_LIGHT : bg, 1),
            new EmptyBorder(8, 16, 8, 16)));
        return button;
    }

    private void addButtonListeners() {
        createMatchButton.addActionListener(e -> onCreateMatchClicked());
        joinMatchButton.addActionListener(e -> onJoinMatchClicked());
        refreshButton.addActionListener(e -> onRefreshClicked());
    }

    private void onCreateMatchClicked() {
        System.out.println("Bottone 'Crea Partita' cliccato");
        if (controller != null) {
            controller.createMatch();
        }
    }

    private void onJoinMatchClicked() {
        System.out.println("Bottone 'Unisciti a Partita' cliccato");
        String selectedMatch = getSelectedMatch();
        if (selectedMatch != null && controller != null) {
            String matchId = selectedMatch.split(" ")[0];
            System.out.println("Match ID estratto: " + matchId);
            controller.joinMatch(matchId);
        }
    }

    private void onRefreshClicked() {
        System.out.println("Bottone 'Ricarica' cliccato");
        if (controller != null) {
            controller.listMatches();
        }
    }

    // Abilita/disabilita "Crea Partita" e "Unisciti a Partita". Usato dal
    // GameController per sbloccarli solo dopo che il server ha confermato la
    // registrazione
    public void setInteractionEnabled(boolean enabled) {
        createMatchButton.setEnabled(enabled);
        joinMatchButton.setEnabled(enabled);
    }

    public void setController(GameController controller) {
        this.controller = controller;
    }

    public void addMatch(String matchInfo) {
        matchesListModel.addElement(matchInfo);
        listCardLayout.show(listCardsPanel, CARD_LIST);
    }

    public void clearMatches() {
        matchesListModel.clear();
        listCardLayout.show(listCardsPanel, CARD_EMPTY);
    }

    public String getSelectedMatch() {
        return matchesList.getSelectedValue();
    }


    private class MatchCellRenderer extends JPanel implements ListCellRenderer<String> {
        private static final long serialVersionUID = 1L;
        private final JLabel titleLabel = new JLabel();
        private final JLabel subtitleLabel = new JLabel();
        private final PillLabel statusPill = new PillLabel();

        MatchCellRenderer() {
            setLayout(new BorderLayout(10, 0));
            setBorder(new EmptyBorder(8, 12, 8, 12));

            titleLabel.setFont(titleLabel.getFont().deriveFont(Font.BOLD, 14f));
            subtitleLabel.setFont(subtitleLabel.getFont().deriveFont(Font.PLAIN, 12f));

            JPanel textPanel = new JPanel();
            textPanel.setOpaque(false);
            textPanel.setLayout(new BoxLayout(textPanel, BoxLayout.Y_AXIS));
            textPanel.add(titleLabel);
            textPanel.add(subtitleLabel);

            statusPill.setFont(statusPill.getFont().deriveFont(Font.BOLD, 11f));
            statusPill.setForeground(Color.WHITE);
            statusPill.setBorder(new EmptyBorder(4, 10, 4, 10));

            add(textPanel, BorderLayout.CENTER);
            add(statusPill, BorderLayout.EAST);
        }

        @Override
        public Component getListCellRendererComponent(JList<? extends String> list, String value,
                int index, boolean isSelected, boolean cellHasFocus) {
            Matcher matcher = MATCH_INFO_PATTERN.matcher(value);
            if (matcher.matches()) {
                String matchId = matcher.group(1);
                String creatorUsername = matcher.group(2);
                String status = matcher.group(3);

                if ("active".equalsIgnoreCase(status)) {
                    // Mentre una partita e' in corso, non si puo' piu' unire: mostriamo
                    // un badge grigio scuro "IN CORSO"
                    titleLabel.setText("Partita di " + creatorUsername);
                    subtitleLabel.setText("In corso, non disponibile");
                    statusPill.setText("IN CORSO");
                    statusPill.setPillColor(PILL_ACTIVE);
                } else {
                    titleLabel.setText(creatorUsername + " ha avviato una partita");
                    subtitleLabel.setText("Clicca per partecipare");
                    if ("pending".equalsIgnoreCase(status)) {
                        // Qualcuno sta gia' valutando se unirsi: si puo'
                        // comunque richiedere, si finira' in coda lato server.
                        statusPill.setText("RICHIESTA IN CORSO");
                    } else {
                        statusPill.setText("IN ATTESA");
                    }
                    statusPill.setPillColor(PILL_WAITING);
                }
            } else {
                titleLabel.setText(value);
                subtitleLabel.setText(" ");
                statusPill.setText("");
            }

            if (isSelected) {
                setBackground(ACCENT);
                titleLabel.setForeground(Color.WHITE);
                subtitleLabel.setForeground(new Color(220, 230, 255));
            } else {
                setBackground(index % 2 == 0 ? Color.WHITE : new Color(250, 250, 251));
                titleLabel.setForeground(TEXT_DARK);
                subtitleLabel.setForeground(TEXT_MUTED);
            }
            setOpaque(true);
            return this;
        }
    }

    private static class PillLabel extends JLabel {
        private static final long serialVersionUID = 1L;
        private Color pillColor = PILL_WAITING;

        PillLabel() {
            setOpaque(false);
            setHorizontalAlignment(SwingConstants.CENTER);
        }

        void setPillColor(Color color) {
            this.pillColor = color;
        }

        @Override
        protected void paintComponent(Graphics g) {
            Graphics2D g2 = (Graphics2D) g.create();
            g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            g2.setColor(pillColor);
            g2.fillRoundRect(0, 0, getWidth(), getHeight(), getHeight(), getHeight());
            g2.dispose();
            super.paintComponent(g);
        }
    }
}
