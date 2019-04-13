//============================================================================
// Name        : Client.cpp
// Author      : Vikturek
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
using namespace std;

#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // htons(), htonl()
#include <strings.h> // bzero()
#include <sys/types.h>
#include <netdb.h>
#include <string.h>


int main(int argc, char **argv) {
	if(argc < 3){
		cerr << "Usage: client address port" << endl;
		return -1;
	}

	int port = atoi(argv[2]);

	// Vytvoreni koncoveho bodu spojeni
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if(s < 0){
		perror("Nemohu vytvorit socket!");
		return -1;
	}

	socklen_t sockAddrSize;
	struct sockaddr_in serverAddr;
	sockAddrSize = sizeof(struct sockaddr_in);
	bzero((char *) &serverAddr, sockAddrSize);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	struct hostent *host;
	host = gethostbyname(argv[1]); // <netdb.h>
	memcpy(&serverAddr.sin_addr, host->h_addr,
	host->h_length); // <string.h>

	// Pripojeni ke vzdalenemu serveru
	if(connect(s, (struct sockaddr *) &serverAddr, sockAddrSize) < 0){
		perror("Nemohu spojit!");
		close(s);
		return -1;
	}

#define BUFFER_SIZE 10240
	char buffer[BUFFER_SIZE];
	if(send(s, "Mnau!\a\b", 7, 0) < 0){
			perror("Nemohu odeslat data!");
			close(s);
			return -3;
		}
	while(true){
		cout << "> ";
		cin.getline (buffer, BUFFER_SIZE);
		if(send(s, buffer, strlen(buffer), 0) < 0){
			perror("Nemohu odeslat data!");
			close(s);
			return -3;
		}
		// Kdyz poslu "konec", ukoncim spojim se serverem
		if(string("konec") == buffer){
			break;
		}
	}


	close(s);
	return 0;
}
