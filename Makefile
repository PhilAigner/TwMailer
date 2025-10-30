CXX := g++
CXXFLAGS := -c -Wall
LDFLAGS := -luuid -pthread
LIBS=-lldap -llber

all: client server

client: client.o
	$(CXX) client.o -o client

server: server.o
	$(CXX) server.o -o server $(LDFLAGS) ${LIBS}

client.o: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp

server.o: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp

clean:
	rm -f *.o client server

runc: all
	./client

runs: all
	./server