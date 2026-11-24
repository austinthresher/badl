CC         := gcc
CFLAGS     := -Wall
TARGET     := tascomp
SRCS       := main.c arena.c
INCS       := arena.h

all: $(TARGET)

$(TARGET): CFLAGS += -O3
$(TARGET): $(SRCS) $(INCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

debug: CFLAGS += -g -DDEBUG -O0
debug: $(SRCS) $(INCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
