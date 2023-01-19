CFLAGS  = -Wall -Wextra -Werror -pedantic  
CC      = clang $(CFLAGS) 
TARGET  = split

split:$(TARGET)
all:$(TARGET)

$(TARGET): $(TARGET).o
	$(CC) -o $(TARGET) $(TARGET).o

$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -c $(TARGET).c

clean:
	rm $(TARGET) $(TARGET).o