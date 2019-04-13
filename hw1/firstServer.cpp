#include <cstdlib>
#include <cstdio>
#include <sys/socket.h> // socket(), bind(), connect(), listen()
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h> // close(), read(), write()
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // htons(), htonl()
#include <strings.h> // bzero()
#include <chrono>
#include <string.h>
#include <thread>
#include <iostream>

using namespace std;
#define BUFFER_SIZE 1024

int setNonblock(int fd){
	int flags;
	#if defined (O_NONBLOCK)
		if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
			flags = 0;
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	#else
		flags = 1;
		return ioctl(fd, FIOBIO, &flags);
	#endif
}


class CRobotClient {
public:
	CRobotClient(int socket) : m_Socket(socket) {}
	void authentication();
	void moveToGoal();
	void pickUpMessage();
	int receivePackage(char * buff, int & recvSize, struct timeval * tv);

private:
	int m_Socket;
	char m_Buffer[2054];
};

int CRobotClient::receivePackage(char * buffer, int & recvSize, struct timeval * tv) {
	fd_set set;
	while(true){
		int retval;
		FD_ZERO(&set);
		FD_SET(m_Socket, &set);		
		retval = select(m_Socket + 1, &set, NULL, NULL, tv);
		if(retval < 0){
			perror("Chyba v select()");
			close(m_Socket);
			return -1;
		}
		if(!FD_ISSET(m_Socket, &set)){
			// Zde je jasne, ze funkce select() skoncila cekani kvuli timeoutu.
			cout << "Connection timeout!" << endl;
			close(m_Socket);
			return 0;
		}
		int bytesRead = recv(m_Socket, buffer, BUFFER_SIZE, 0);
		if(bytesRead <= 0){
			perror("Chyba pri cteni ze socketu.");
			close(m_Socket);
			return -3;
		}
		buffer[bytesRead] = '\0';
		recvSize = bytesRead;	
		cout << buffer << endl;	
		break;
	}
	return 0;
}


void CRobotClient::authentication(){
	cout << "Authentication, socket: " << m_Socket << endl;
	int recvSize = 0;
	char sendBuff[256];
	//receiving the username
	bzero(&m_Buffer, sizeof(m_Buffer));
	bzero(&sendBuff, sizeof(sendBuff));

	
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
			
	
	receivePackage(m_Buffer, recvSize, &timeout);
	cout << recvSize << endl;
	if (recvSize > 2 && m_Buffer[recvSize-1] == '\b'
			&& m_Buffer[recvSize-2] == '\a'){
		int hashCode = 0, messageSize = 0;
		for (int i = 0; i < recvSize-2; ++i) hashCode += m_Buffer[i];
		hashCode *= 1000;
		hashCode %= 65536;
		messageSize = sprintf(sendBuff, "%d\a\b", hashCode);
		cout << hashCode << endl;
		send(m_Socket, sendBuff, messageSize, MSG_NOSIGNAL);
		
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}

void CRobotClient::moveToGoal(){
	cout << "Moving to the goal, socket: " << m_Socket << endl; 
	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}

void CRobotClient::pickUpMessage(){
	cout << "Picking up a message, socket: " << m_Socket << endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}


void socketThread(void * dummyArg){
	int slaveSocket = (int) (intptr_t) dummyArg;
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
		perror("Error listen()");
		close(masterSocket);
		return -1;
	}

	struct sockaddr_in remoteAddr;
	socklen_t addrSize;


    while(true){
        int slaveSocket = accept(masterSocket, 
				(struct sockaddr *) &remoteAddr, &addrSize);
	if (slaveSocket < 0){
		perror("Unable to accept a slave socket");
		return -1;
	}
	
	thread currentThread (socketThread, (void *)(intptr_t) slaveSocket);
	currentThread.detach();
    }

  close(masterSocket);
  return 0;

}




