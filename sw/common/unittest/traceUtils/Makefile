TARGET = traceUtils
BIN = ./bin

CC = gcc
CCFLAGS = -I../../if -lpthread


all: TARGET

TARGET: traceUtils.c
	mkdir -p $(BIN)
	$(CC) -o $(BIN)/$(TARGET) $< $(CCFLAGS)

run:
	$(BIN)/$(TARGET)

clean:
	rm -rf $(BIN)