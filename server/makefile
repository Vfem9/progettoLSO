# Compilatore e flag
CC = gcc
CFLAGS = -I./include -Wall -Wextra -pthread
LDFLAGS = -pthread -lws2_32

# Directory
SRC_DIR = src
INCLUDE_DIR = include
BIN_DIR = bin
OBJ_DIR = obj

# File sorgente e object
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/game.c $(SRC_DIR)/match.c $(SRC_DIR)/network.c $(SRC_DIR)/protocol.c $(SRC_DIR)/sync.c
OBJECTS = $(OBJ_DIR)/main.o $(OBJ_DIR)/game.o $(OBJ_DIR)/match.o $(OBJ_DIR)/network.o $(OBJ_DIR)/protocol.o $(OBJ_DIR)/sync.o
EXECUTABLE = $(BIN_DIR)/forza4_server

# Target di default
all: $(EXECUTABLE)

# Crea l'eseguibile linkando gli object file
$(EXECUTABLE): $(OBJECTS)
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Server compilato: $@"

# Compila ogni file .c in un file .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compilato: $<"

# Pulisce i file compilati
clean:
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist $(BIN_DIR) rmdir /s /q $(BIN_DIR)
	@echo "Pulizia completata"

# Compila tutto da zero
rebuild: clean all

# Run il server (dopo la compilazione)
run: $(EXECUTABLE)
	$(EXECUTABLE)

# Help
help:
	@echo Comandi disponibili:
	@echo   make all       - Compila il server
	@echo   make clean     - Rimuove i file compilati
	@echo   make rebuild   - Pulisce e ricompila
	@echo   make run       - Compila e esegue il server
	@echo   make help      - Mostra questo messaggio

.PHONY: all clean rebuild run help