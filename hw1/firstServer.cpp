#include <iostream>
using namespace std;

#include <cstdlib>
#include <cstdio>
#include <sys/socket.h> // socket(), bind(), connect(), listen()
#include <unistd.h> // close(), read(), write()
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // htons(), htonl()
#include <strings.h> // bzero()
#include <string.h>
#include <thread>
class CRobotClient {

public:
	CRobotClient(int socket) : m_Socket(socket) {}
	void authentication();
	void moveToGoal();
	void pickUpMessage();
private:
	int m_Socket;
}

void CRobotClient::authentication(){
	cout << "Authentication" << endl;
}

void CRobotClient::moveToGoal(){
	cout << "Moving to the goal" << endl;
}

void CRobotClient::pickUpMessage(){
	cout << "Picking up a message" << endl;

}


void * socketThread(void * dummyArg){
	int slaveSocket = *((int *) dummyArg);
	CRobotClient currentRobot(slaveSocket);
	currentRobot.authentication();
	currentRobot.moveToGoal();
	currentRobot.pickUpMessage();
	close(slaveSocket);
}

int main (int argc, char ** argv){
	
	if(argc < 2){
		cerr << "Usage: server port" << endl;
		return -1;
	}
	
	// Create master socket
	int masterSocket = socket(AF_INET, /*IPv4*/
								SOCK_STREAM,
								IPPROTO_TCP);
	if (masterSocket < 0){
		perror("Unable to create a master socket");
		return -1;
	}
	int port = atoi(argv[1]);
	if (port == 0){
		cerr << "Usage: server port" << endl;
		close(masterSocket);
		return -1;
	}
	struct sockaddr_in sockAddr;
	bzero(&sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(port);
	sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(masterSocket, (struct sockaddr *) &sockAddr,
						sizeof(sockAddr)) < 0){
		perror("Error bind()");
		close(masterSocket);
		return -1;
	}
	
	if(listen(masterSocket, SOMAXCONN) < 0){
		perror("Error listen()!");
		close(masterSocket);
		return -1;
	}

	struct sockaddr_in remoteAddr;
	socklen_t addrSize;

    while(1){
        int slaveSocket = accept(masterSocket, 
				(struct sockaddr *) &remoteAddr, &addr_size);

	if (slaveSocket < 0){
		perror("Unable to accept a slave socket");
		return -1;
	}
	
	thread currentThread (socketThread, slaveSocket);
	currentThread.detach();
    }

  return 0;

}



