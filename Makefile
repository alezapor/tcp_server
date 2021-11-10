all:
	g++ -std=c++11 -Wall -pedantic zaporole_ht1.cpp -o server -lpthread -g
	g++ -std=c++11 -Wall -pedantic firstRobot.cpp -o client -lpthread -g

clean:
	rm server
	rm client
