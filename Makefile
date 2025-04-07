CC = gcc
CFLAGS = -Wall -pthread
TARGET = chash
OBJS = test.o hashTable.o rwlock.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS)
