CFLAGS += -O2 -g -Wall
test: btree.o main.o
	$(CC) $(CFLAGS) btree.o main.o -o test
btree.o: btree.h
clean:
	rm -rf *.o test
