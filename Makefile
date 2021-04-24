all: server client

client: packet.o client.o 
	g++ -g -o client client.o packet.o -pthread

client.o: src/client.cpp include/packet.hpp
	g++ -g -c -o client.o src/client.cpp -pthread
	
server: Row.o MasterTable.o packet.o server.o 
	g++ -g -o server server.o packet.o Row.o MasterTable.o -pthread

server.o: src/server.cpp include/packet.hpp
	g++ -g -c -o server.o src/server.cpp -pthread

packet.o: src/packet.cpp include/packet.hpp
	g++ -g -c -o packet.o src/packet.cpp -pthread

MasterTable.o: src/MasterTable.cpp include/MasterTable.hpp
	g++ -g -c -o MasterTable.o src/MasterTable.cpp -pthread

Row.o: src/Row.cpp include/Row.hpp
	g++ -g -c -o Row.o src/Row.cpp -pthread

clean:
	rm -rf *.o *~ server client
