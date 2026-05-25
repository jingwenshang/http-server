CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread -I./include
LDFLAGS = -pthread

SRC     = src/main.c src/threadpool.c src/http.c
OBJ     = $(SRC:.c=.o)
TARGET  = server

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

src/%.o: src/%.c include/server.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

# Quick smoke test: start server, fetch index, kill server
test: $(TARGET)
	@echo "=== Starting server on port 9999 ==="
	@./$(TARGET) -port 9999 -document_root ./docroot &
	@sleep 1
	@echo "=== Fetching index.html ==="
	@curl -s http://localhost:9999/index.html || echo "FAILED"
	@echo ""
	@echo "=== Fetching 404 ==="
	@curl -s -o /dev/null -w "%{http_code}" http://localhost:9999/nonexistent.html
	@echo ""
	@echo "=== Killing server ==="
	@pkill -f "./server -port 9999" || true
	@echo "=== Done ==="
