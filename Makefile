BIN_DIR := bin
BIN := $(BIN_DIR)/mvp
SRC := src/main.c src/queue.c
CFLAGS := -O2 -Wall -Wextra -std=c11 -pthread

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

clean:
	rm -rf $(BIN_DIR) metrics/*.csv plots/*.png latencies_ns.txt stats_*.txt
