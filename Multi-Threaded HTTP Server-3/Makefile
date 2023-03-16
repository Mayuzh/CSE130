CFLAGS 	= -Wall -Wextra -Werror -Wpedantic 
CC 		= clang -pthread
TARGET 	= httpserver

OBJ:= $(patsubst %.c,%.o,$(wildcard *.c))

all: $(TARGET)
httpserver: $(TARGET)

$(TARGET): $(OBJ)
		$(CC) -o $@ ${OBJ}

%.o: %.c
		$(CC) $(CFLAGS) -o $@ -c $<

valgrind:
		valgrind ./$(TARGET) -A

clean:
		rm -f $(TARGET) *.o