TARGET = shell
BIN = ./bin

CFLAGS = -g -Wall -Werror -Wextra
CC = gcc

all: $(BIN)/$(TARGET)

$(BIN)/$(TARGET): shell.c
	@mkdir -p $(BIN)
	@echo "  CC  $(BIN)/$(TARGET)"
	@$(CC) $(CFLAGS) -o $@ $<

run:
	$(BIN)/$(TARGET)

val:
	sudo valgrind --leak-check=yes --leak-check=full --show-leak-kinds=all $(BIN)/$(TARGET)

clean:
	rm -rf $(BIN)