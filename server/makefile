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

# FIX: "mkdir -p" e "rm -rf" sono comandi Unix. Se make viene eseguito su
# Windows con una toolchain MinGW "pura" (senza una shell unix tipo
# MSYS2/Git-Bash in PATH), le regole vengono lanciate tramite cmd.exe, che
# non conosce questi comandi/flag e la build si interrompe con errori tipo
# "Sottodirectory o file -p gia' esistente". Qui rileviamo il sistema
# operativo (la variabile OS e' impostata automaticamente da Windows) e
# usiamo la sintassi giusta in ciascun caso, cosi' lo stesso Makefile
# funziona sia su Windows (cmd) sia su Linux/Mac/Docker.
ifeq ($(OS),Windows_NT)
    EXECUTABLE := $(EXECUTABLE).exe
    MKDIR_BIN = if not exist $(BIN_DIR) mkdir $(BIN_DIR)
    MKDIR_OBJ = if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
    RMDIR = if exist $(OBJ_DIR) rmdir /S /Q $(OBJ_DIR) & if exist $(BIN_DIR) rmdir /S /Q $(BIN_DIR)
    RUN_CMD = $(subst /,\,$(EXECUTABLE))
    # FIX: su Windows le funzioni socket (select, socket, bind, send, recv...)
    # non stanno nella libc ma nella libreria Winsock. network.c prova a
    # linkarla da solo con "#pragma comment(lib, ws2_32.lib)", ma quel pragma
    # e' specifico di MSVC: GCC/MinGW lo ignora silenziosamente (da qui il
    # warning "ignoring '#pragma comment'"), quindi il linker non trova mai
    # quelle funzioni ("undefined reference to `__imp_select'" ecc.). Va
    # aggiunta esplicitamente al link con -lws2_32.
    LDFLAGS += -lws2_32
else
    MKDIR_BIN = mkdir -p $(BIN_DIR)
    MKDIR_OBJ = mkdir -p $(OBJ_DIR)
    RMDIR = rm -rf $(OBJ_DIR) $(BIN_DIR)
    RUN_CMD = ./$(EXECUTABLE)
endif

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(MKDIR_BIN)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo Server compilato: $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(MKDIR_OBJ)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo Compilato: $<

clean:
	$(RMDIR)
	@echo Pulizia completata

rebuild: clean all

run: $(EXECUTABLE)
	$(RUN_CMD)

help:
	@echo Comandi disponibili:
	@echo   make all       - Compila il server
	@echo   make clean     - Rimuove i file compilati
	@echo   make rebuild   - Pulisce e ricompila
	@echo   make run       - Compila e esegue il server
	@echo   make help      - Mostra questo messaggio

.PHONY: all clean rebuild run help
