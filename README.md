# Forza 4 Multi-Client

Progetto universitario di Laboratorio di Sistemi Operativi.

## Descrizione

Server multi-client per giocare a Forza 4.
- **Server**: C con thread + mutex
- **Client**: Java + Swing
- **Comunicazione**: Socket TCP + JSON
- **Containerizzazione**: Docker Compose

## Autori

- Antonio Tommaselli N86004955
- Luigi Ariola

## Prerequisiti

- GCC (o compilatore C)
- Java JDK 11+
- Maven
- Docker & Docker Compose
- Git

## Come Eseguire

### Build Server C
```bash
cd server
make clean && make
```

### Build Client Java
```bash
cd ../client
mvn clean package
```

### Run con Docker
```bash
cd ..
docker-compose up --build
```

Server ascolta su `localhost:5000`.

### Eseguire il Client
```bash
cd client
java -jar target/forza4-client-1.0.jar
```

## Struttura Progetto