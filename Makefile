CC=g++
CFLAGS=-Wall -Wextra -O2

all: oss worker

oss: oss.cpp
	$(CC) $(CFLAGS) -o oss oss.cpp

worker: worker.cpp
	$(CC) $(CFLAGS) -o worker worker.cpp

clean:
	rm -f oss worker
