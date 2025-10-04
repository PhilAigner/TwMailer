all: client.o server.o
	g++ client.o -o client
	g++ server.o -o server

client.o: client.cpp
	g++ -c -Wall client.cpp

server.o: server.cpp
	g++ -c -Wall server.cpp

clean:
	rm -f *.o client server

runc: all
	./client

runs: all
	./server