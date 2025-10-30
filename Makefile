CXX := g++
CXXFLAGS := -Wall
LDFLAGS := -luuid -pthread
LIBS := -lldap -llber

all: client server

client: client.cpp clientfunctions.cpp mypw.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o client

server: server.cpp serverfunctions.cpp ldap.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o server $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o client server

runc: all
	./client

runs: all
	./server