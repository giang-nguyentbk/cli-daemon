ROOT_DIR := $(shell git rev-parse --show-toplevel)
TARGET = shell
BIN = ./bin

CFLAGS = -g -Wall -Werror -Wextra
CC = gcc

INC_PATHS	:= \
		-I$(ROOT_DIR)/sw/common/if

all: $(BIN)/$(TARGET)

$(BIN)/$(TARGET): shell.c
	@mkdir -p $(BIN)
	@echo "  CC  $(BIN)/$(TARGET)"
	@$(CC) $(CFLAGS) $(INC_PATHS) -o $@ $<

run:
	@$(BIN)/$(TARGET)

val:
	sudo valgrind --leak-check=yes --leak-check=full --show-leak-kinds=all $(BIN)/$(TARGET)

clean:
	rm -rf $(BIN)