CC = gcc
CFLAGS = -I./include -Wall -Wextra -pthread
LDFLAGS = -pthread

SRC_DIR = src
INCLUDE_DIR = include
BIN_DIR = bin
OBJ_DIR = obj

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/game.c $(SRC_DIR)/match.c $(SRC_DIR)/network.c $(SRC_DIR)/protocol.c $(SRC_DIR)/sync.c
OBJECTS = $(OBJ_DIR)/main.o $(OBJ_DIR)/game.o $(OBJ_DIR)/match.o $(OBJ_DIR)/network.o $(OBJ_DIR)/protocol.o $(OBJ_DIR)/sync.o
EXECUTABLE = $(BIN_DIR)/forza4_server

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	mkdir -p $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "✓ Server compilato: $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "✓ Compilato: $<"

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "✓ Pulizia completata"

rebuild: clean all

run: $(EXECUTABLE)
	./$(EXECUTABLE)

help:
	@echo "Comandi disponibili:"
	@echo "  make all       - Compila il server"
	@echo "  make clean     - Rimuove i file compilati"
	@echo "  make rebuild   - Pulisce e ricompila"
	@echo "  make run       - Compila e esegue il server"
	@echo "  make help      - Mostra questo messaggio"

.PHONY: all clean rebuild run help