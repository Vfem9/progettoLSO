# Forza 4 - Progetto Laboratorio di Sistemi Operativi

Applicazione client-server multi-utente per giocare a Forza 4. Il server e'
scritto in C (socket TCP, un thread per client) ed e' pensato per essere
eseguito tramite Docker/Docker Compose. Il client e' scritto in Java (Swing)
e comunica con il server tramite lo stesso protocollo a socket (nessun
websocket).

## Struttura del progetto

```
forza4/
├── docker-compose.yml
├── server/                  # Server C
│   ├── Dockerfile
│   ├── Makefile
│   ├── include/             # Header (.h)
│   └── src/                 # Sorgenti (.c)
└── client/                  # Client Java (Maven)
    ├── pom.xml
    └── src/main/java/com/forza4/
        ├── Main.java
        ├── controller/      # GameController (logica applicativa)
        ├── model/           # GameModel (stato lato client)
        ├── network/         # NetworkClient, MessageProtocol
        └── view/            # Swing: MainFrame, LobbyPanel, GamePanel, ...
```

## Requisiti

- **Server**: Docker e Docker Compose (non serve avere gcc/make installati
  sulla macchina host: la compilazione avviene dentro il container).
- **Client**: JDK 11 o superiore e Maven.

Il server e il client possono girare sulla stessa macchina (test in locale)
oppure su macchine diverse: in tal caso basta cambiare l'indirizzo del
server in `client/src/main/java/com/forza4/controller/GameController.java`
(costruttore di `NetworkClient`, attualmente `"localhost", 5000`).

## Avvio del server

Dalla cartella principale del progetto (dove si trova `docker-compose.yml`):

```
docker compose up --build
```

Il server si mette in ascolto sulla porta **5000** (esposta anche
all'host). Per fermarlo:

```
docker compose down
```

Se dopo aver modificato i sorgenti del server il comportamento non sembra
cambiare, e' probabile che Docker stia riusando un'immagine in cache. In tal
caso forzare un rebuild completo:

```
docker compose down
docker compose build --no-cache
docker compose up
```

## Avvio del client

Dalla cartella `client/`:

```
cd client
mvn clean package
java -jar target/forza4-client-1.0.jar
```

Va lanciata **un'istanza per ogni giocatore** che vuole partecipare (sono
supportati piu' di due client contemporaneamente: chi non gioca vede in
lobby le partite disponibili e quelle in corso). All'avvio, dopo che la
finestra si e' aperta, viene chiesto uno username: la richiesta si ripete
finche' non se ne inserisce uno non vuoto.

## Come si gioca

1. Ogni client si registra con un username scelto liberamente.
2. Un client crea una partita (bottone "Crea Partita"): la partita compare
   nella lobby di tutti gli altri client connessi, con lo stato "IN ATTESA".
3. Un altro client seleziona la partita e clicca "Unisciti a Partita":
   questo invia una **richiesta** di partecipazione, non un ingresso
   immediato.
4. Il creatore riceve un popup con la richiesta e puo' accettarla o
   rifiutarla. Se altri client richiedono di partecipare nel frattempo,
   restano in coda e vengono proposti al creatore uno alla volta, in
   ordine.
5. Se accettata, la partita inizia per i due giocatori coinvolti. Gli altri
   client connessi la vedono in lobby con lo stato "IN CORSO" (non e' piu'
   possibile richiedere di parteciparvi).
6. A fine partita (vittoria, sconfitta o pareggio), i due giocatori
   ricevono l'esito. In caso di vittoria/sconfitta il perdente torna
   automaticamente in lobby, mentre il vincitore puo' scegliere di aprire
   una nuova sessione (di cui diventa proprietario, aperta a chiunque in
   lobby). In caso di pareggio, la partita riparte con la stessa coppia di
   giocatori solo se **entrambi** scelgono di continuare.

## Note

- Il server tiene tutto lo stato in memoria: riavviandolo si perdono tutte
  le partite e le registrazioni in corso (i client devono riconnettersi).
- Il protocollo applicativo e i dettagli implementativi (sincronizzazione,
  macchina a stati delle partite, gestione delle richieste di
  partecipazione, fedelta' del rigioco alla traccia, ecc.) sono descritti
  nella documentazione di progetto allegata (`documentazione.pdf`).
