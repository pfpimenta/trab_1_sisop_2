CFLAGS = -g -Wall
LDFLAGS = -pthread

all: server client

client: packet.o client.o 
	g++ $(CFLAGS) -o client client.o packet.o $(LDFLAGS)

client.o: src/client.cpp include/packet.hpp
	g++ $(CFLAGS) -o client.o -c src/client.cpp $(LDFLAGS)
	
server: Row.o MasterTable.o packet.o server.o session.o
	g++ $(CFLAGS) -o server server.o packet.o Row.o MasterTable.o $(LDFLAGS)

server.o: src/server.cpp include/packet.hpp
	g++ $(CFLAGS) -o server.o -c src/server.cpp $(LDFLAGS)

packet.o: src/packet.cpp include/packet.hpp
	g++ $(CFLAGS) -o packet.o -c src/packet.cpp $(LDFLAGS)

MasterTable.o: src/MasterTable.cpp include/MasterTable.hpp
	g++ $(CFLAGS) -o MasterTable.o -c src/MasterTable.cpp $(LDFLAGS)

Row.o: src/Row.cpp include/Row.hpp
	g++ $(CFLAGS) -o Row.o -c src/Row.cpp $(LDFLAGS)

session.o: src/session.cpp include/session.hpp
	g++ $(CFLAGS) -o session.o -c src/session.cpp $(LDFLAGS)

clean:
	rm -rf *.o *~ server client

