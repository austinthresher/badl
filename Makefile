CC         := gcc
CFLAGS     := -Wall
TARGET     := badlc
SRCS       := arena.c str.c ast.c
INCS       := arena.h str.h ast.h

all: $(TARGET)

$(TARGET): CFLAGS += -O3
$(TARGET): main.c $(SRCS) $(INCS)
	$(CC) $(CFLAGS) main.c $(SRCS) -o $@

debug: CFLAGS += -g -DDEBUG -O0
debug: main.c $(SRCS) $(INCS)
	$(CC) $(CFLAGS) main.c $(SRCS) -o $(TARGET)

test: CFLAGS += -g -DDEBUG -O0
test: test.c $(SRCS) $(INCS)
	$(CC) $(CFLAGS) test.c $(SRCS) -o test

clean:
	rm -f $(TARGET) test

.PHONY: all clean
