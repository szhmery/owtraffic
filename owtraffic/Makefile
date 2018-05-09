#all : owtraffic
##all : server client
#
#owtraffic: owtraffic.cpp
#	g++ -pthread -o owtraffic owtraffic.cpp
#
#server: server.cpp udp.h
#	g++ -pthread -o server server.cpp
#
#client : client.cpp	udp.h
#	g++ -o client client.cpp
#	
#clean:
#	rm -f server client

CC = g++
CPPFLAG = -Wall -g -pthread -std=c++11

TARGET = $(patsubst %.cpp, %.out, $(wildcard *.cpp))
HEADER = $(wildcard headers/*.h)

%.out : %.cpp $(HEADER)
	$(CC) -c $(CPPFLAG) $< -o $@

.PHONY : all clean

owtraffic : $(TARGET)
	$(CC) -pthread  $(CPPFLAG) $(TARGET) -o $@

all : owtraffic

clean : 
	rm -f *.out owtraffic