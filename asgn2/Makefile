CFLAGS  = -Wall -Wextra -Werror -pedantic  
CC      = clang $(CFLAGS) 
TARGET  = httpserver

server:$(TARGET)
all:$(TARGET)

$(TARGET): $(TARGET).o
	$(CC) -o $(TARGET) $(TARGET).o

$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -c $(TARGET).c

clean:
	rm $(TARGET) $(TARGET).o