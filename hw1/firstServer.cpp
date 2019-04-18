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
#include <stack>
#include <set>

using namespace std;
#define BUFFER_SIZE 1024
#define SERVER_CODE 54621
#define CLIENT_CODE 45328

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

char * subString(char * str1){
	for (int i = 0; i < BUFFER_SIZE-1; i++){
		if (str1[i]=='\a' && str1[i+1]=='\b') return str1+i+2;
	}
	return NULL;
}

enum Direction {LEFT, UP, RIGHT, DOWN};

class CCoordinates {

public:
	CCoordinates() : x(0), y(0), dir(LEFT) {}
	void setX(int x) {this->x = x;} 
	void setY(int y) {this->y = y;}
	void setDir(int x, int y) {
		if(x == 0){
			if(y>0) this->dir = UP;
			else this->dir = DOWN;
		} 
		else {
			if(x>0) this->dir = LEFT;
			else this->dir = RIGHT;
		}
	} 
	int getX() {return x;}
	int getY() {return y;}
	int getDir() {return dir;}
	void move() {
		switch(dir){
			case LEFT: 
				x--;
				break;
			case RIGHT:
				x++;
				break;
			case UP:
				y++;
				break;
			default:
				y--; break;
			
		}	
	}
	void turnLeft() {
		this->dir = (Direction) ((this->dir + 3) % 4);
	}
	void turnRight() {
		this->dir = (Direction) ((this->dir + 1) % 4);
	}
	bool isInSquare() {
		cout << "Checking..." << x << ":" << y << endl;
		return x <= 2 && x >= -2 && y <= 2 && y >= -2;
	}
	
private:
	int x;
	int y;
	Direction dir;
	
};


class CRobotClient {
public:
	static int const neighs[4][2]; //for DFS

	CRobotClient(int socket) : m_Socket(socket), m_BufferTop(0), m_Coordinates(CCoordinates()) {}
	bool checkMove(char *);
	bool authentication();
	bool moveToGoal();
	void pickUpMessage();
	bool moveTo(int, int, timeval*);
	bool recharging();
	bool tryPickUp(struct timeval * tv);
	bool goLeft(struct timeval * tv);
	bool goRight(struct timeval * tv);
	bool goUp(struct timeval * tv);
	bool move(struct timeval * tv);
	void turnLeft(struct timeval * tv);
	void turnRight(struct timeval * tv);
	bool goDown(struct timeval * tv);
	bool DFS(int i, int j, struct timeval * tv); 
	int receivePackage(int & recvSize, struct timeval * tv, char * receivedPackage, int max);

private:
	int m_Socket;
	int m_BufferTop;
	char m_SendBuffer[BUFFER_SIZE];
	char m_RecvBuffer[BUFFER_SIZE];
	CCoordinates m_Coordinates;
};

bool CRobotClient::recharging(){
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	int recvSize = 0;
	char receivedPackage[BUFFER_SIZE];
	if(receivePackage(recvSize, &timeout, receivedPackage) == -6){
		messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	   	send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
		return false;

	}
	if (recvSize > 2 && memcmp(receivedPackage, "FULL POWER\a\b") != 0){
		cout << "receivedPackage: " << receivedPackage << endl;
		bzero(&m_SendBuffer, sizeof(m_SendBuffer));
		messageSize = sprintf(m_SendBuffer, "302 LOGIC ERROR\a\b");
		send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
		return false;
	}
	return true;
}

int CRobotClient::receivePackage(int & recvSize, struct timeval * tv, char * receivedPackage, int maxSize = 12) {
	char * subStr;
	char copyArray[BUFFER_SIZE] = {0};
	fd_set set;
	while(true){
		int retval;
		subStr = subString(m_RecvBuffer);
		if (subStr != NULL) {
			    recvSize = subStr-m_RecvBuffer;
			    cout << "GOT:" << recvSize << endl;
			    memcpy(receivedPackage, m_RecvBuffer, 
						recvSize);
			    memcpy(copyArray, subStr, BUFFER_SIZE-recvSize);
			    memcpy(m_RecvBuffer, copyArray, BUFFER_SIZE);	    
			    cout << "BUFF:(0) " << (int)m_RecvBuffer[1] <<endl;
			    m_BufferTop -= recvSize;
			    break;
		}	
		FD_ZERO(&set);
		FD_SET(m_Socket, &set);		
		retval = select(m_Socket + 1, &set, NULL, NULL, tv);
		if(retval < 0){
			perror("Chyba v select()");
			close(m_Socket);
			return -1;
		}
		if(!FD_ISSET(m_Socket, &set)){
			cout << "Connection timeout!" << endl;
			close(m_Socket);
			return 0;
		}
		unsigned int counter = maxSize; 
        	while(counter > 0) {
            		int res = recv(m_Socket, m_RecvBuffer + m_BufferTop + maxSize - counter, counter, 0);
           		if(res > 0) counter -= res; 
			if(res == -1) {
				close(m_Socket);
				return -5;
							
			} 
			subStr = subString(m_RecvBuffer);
			cout << "REC" << res << endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			if (subStr != NULL) {
			    recvSize = subStr-m_RecvBuffer;
			    cout << "GOT:" << recvSize << endl;
			    memcpy(receivedPackage, m_RecvBuffer, 
						recvSize);
			    memcpy(copyArray, subStr, BUFFER_SIZE-recvSize);
			    memcpy(m_RecvBuffer, copyArray, BUFFER_SIZE);	    
			    m_BufferTop -= recvSize;
			    cout << "BUFF:(1) " << (int)m_RecvBuffer[1] <<endl;
			    if(memcmp(receivedPackage, "RECHARGING\a\b") == 0){
				if(recharging()) {
					receivePackage(recvSize,tv,receivedPackage, maxSize);
					return 0;
				}
				else return -7;
			    }
			    //TODO
				break;
			}	
       		}
		if(counter == 0 && subStr == NULL) return -6;
		m_BufferTop += maxSize - counter;
		if(counter == (unsigned) maxSize){
			perror("Chyba pri cteni ze socketu.");
			close(m_Socket);
			return -3;
		}	
		cout << receivedPackage << endl;	
		break;
	}
	return 0;
}

bool CRobotClient::goUp(struct timeval * tv){
	switch(m_Coordinates.getDir()){
		case UP:
			if (!move(tv)) return false;
			break;
		case RIGHT: case DOWN:
			turnLeft(tv);
			break;
		default:
			turnRight(tv);
	}
	return true;
}

bool CRobotClient::goDown(struct timeval * tv){
	switch(m_Coordinates.getDir()){
		case DOWN:
			if (!move(tv)) return false;
			break;
		case LEFT: case UP:
			turnLeft(tv);
			break;
		default:
			turnRight(tv);
	}
	return true;
}

bool CRobotClient::goLeft(struct timeval * tv){
	switch(m_Coordinates.getDir()){
		case LEFT:
			if (!move(tv)) return false;
			break;
		case RIGHT: case UP:
			turnLeft(tv);
			break;
		default:
			turnRight(tv);
	}
	return true;
}

bool CRobotClient::goRight(struct timeval * tv){
	switch(m_Coordinates.getDir()){
		case RIGHT:
			if (!move(tv)) return false;
			break;
		case LEFT: case UP:
			turnRight(tv);
			break;
		default:
			turnLeft(tv);
	}
	return true;
}

bool CRobotClient::tryPickUp(struct timeval * timeout){
	int messageSize, recvSize = 0;
	char receivedPackage[BUFFER_SIZE];
	bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
	bzero(&m_SendBuffer, sizeof(m_SendBuffer));
	messageSize = sprintf(m_SendBuffer, "105 GET MESSAGE\a\b");
	send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	if(receivePackage(recvSize, timeout, receivedPackage, 100) == -6){
		messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	   	send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
		return true;

	}
	if (recvSize > 2){
		cout << "receivedPackage: " << receivedPackage << endl;
		bzero(&m_SendBuffer, sizeof(m_SendBuffer));
		messageSize = sprintf(m_SendBuffer, "106 LOGOUT\a\b");
		send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
		
		return true;
	}
	 
	bzero(&receivedPackage, sizeof(receivedPackage));
	return false;
}

bool CRobotClient::checkMove(char * receivedPackage){
    printf("Parsing the input string '%s'\n", receivedPackage);
    char *token = strtok(receivedPackage, " ");
    if(strcmp(token, "OK")!=0) return false;
    token = strtok(NULL, " ");
    if(strcmp(token, "0")!=0 && strtol(token, NULL, 10) == 0) return false;
    token = strtok(NULL, "\a\b");
    if(strcmp(token, "0")!=0 && strtol(token, NULL, 10) == 0) return false;
    return true;
}

bool CRobotClient::move(struct timeval * tv){
	int x1, y1;
	int recvSize, messageSize;
	char receivedPackage[BUFFER_SIZE];
	char okStr[3] = {0};
	messageSize = sprintf(m_SendBuffer, "102 MOVE\a\b");
	send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	receivePackage(recvSize, tv, receivedPackage);
	if (recvSize > 2){
		if (sscanf(receivedPackage, "%2s%d%d", okStr, &x1, &y1) != 3 || !checkMove(receivedPackage)){
			messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	   	 send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
			return false;
		}
		this->m_Coordinates.move();
	bzero(&receivedPackage, sizeof(receivedPackage));
	cout << "Move (" << x1 << ", " << y1 << ")" <<endl;
	}
	else {
		messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	   	 send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
		return false;
	}
	return true;
}

bool CRobotClient::moveTo(int i, int j, struct timeval * timeout){
	while (true){
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));
		//bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
		bzero(&m_SendBuffer, sizeof(m_SendBuffer));
		if (m_Coordinates.getX() == i && m_Coordinates.getY() == j) break;
		else if (m_Coordinates.getX() != i) {
			if (m_Coordinates.getX() > i){
				if(!this->goLeft(timeout)) return false;
			}else {
				if(!this->goRight(timeout)) return false;
			}
			continue;
		}
		else {
			if (m_Coordinates.getY() > j){
				if(!this->goDown(timeout)) return false;
			}else {
				if(!this->goUp(timeout)) return false;
			}
			continue;
		}
		
	}	
	return true; 
}

void CRobotClient::turnLeft(struct timeval * tv){
	int recvSize, messageSize;
	char receivedPackage[BUFFER_SIZE];
	messageSize = sprintf(m_SendBuffer, "103 TURN LEFT\a\b");
	send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	receivePackage(recvSize, tv, receivedPackage);
	this->m_Coordinates.turnLeft();
	bzero(&receivedPackage, sizeof(receivedPackage));
}

void CRobotClient::turnRight(struct timeval * tv){
	int recvSize, messageSize;
	char receivedPackage[BUFFER_SIZE];
	messageSize = sprintf(m_SendBuffer, "104 TURN RIGHT\a\b");
	send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	receivePackage(recvSize, tv, receivedPackage);
	this->m_Coordinates.turnRight();
	bzero(&receivedPackage, sizeof(receivedPackage));
}

bool CRobotClient::authentication(){
	cout << "Authentication, socket: " << m_Socket << endl;
	int recvSize = 0, hashCode = 0, clientCode = 0, serverCode = 0;
	int messageSize = 0;
	//receiving the username
	bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
	bzero(&m_SendBuffer, sizeof(m_SendBuffer));

	
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	setsockopt(m_Socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
	char receivedPackage[BUFFER_SIZE];
	receivePackage(recvSize, &timeout, receivedPackage);

	if (recvSize > 2){
		messageSize = 0;
		for (int i = 0; i < recvSize-2; ++i) {
			hashCode += receivedPackage[i];
			cout << "HASH + " << (int) receivedPackage[i] << endl;	
		}
		hashCode *= 1000;
		hashCode %= 65536;
		serverCode = (SERVER_CODE + hashCode) % 65536; 
		messageSize = sprintf(m_SendBuffer, "%d\a\b", serverCode);
		cout << serverCode << endl;
		send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
		bzero(&receivedPackage, sizeof(receivedPackage));
		
	}
	else{
	    messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	    send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	    return false;
	}
	recvSize = 0;
	//bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
	bzero(&m_SendBuffer, sizeof(m_SendBuffer));
	receivePackage(recvSize, &timeout, receivedPackage, 7);
	if (recvSize > 2){
		for (int i = 0; i < recvSize-2; i++) {
			if (receivedPackage[i] <'0' || receivedPackage[i] >'9')		
			{
				messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	    			send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	    		return false;	
			}
			clientCode = clientCode * 10
				+ receivedPackage[i]-'0';
		}
		clientCode -= CLIENT_CODE;
		clientCode = (clientCode > 0) ? (clientCode % 65536) : (clientCode + 65536) % 65536;
		if (clientCode == hashCode)		
			messageSize = sprintf(m_SendBuffer, "200 OK\a\b");
		else {
			messageSize = sprintf(m_SendBuffer, "300 LOGIN FAILED\a\b");
			send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
			close(m_Socket);
			return false;
		}
		send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
		cout << clientCode << endl;
		bzero(&receivedPackage, sizeof(receivedPackage));
		
	}
	else{
	    messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	    send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	    return false;

	}
	return true;
	//std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}

bool CRobotClient::moveToGoal(){
	cout << "Moving to the goal, socket: " << m_Socket << endl; 
	int x, y, x1, y1, messageSize = 0, recvSize = 0;
	char okStr[3] = {0};
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	char receivedPackage[BUFFER_SIZE];		
	//bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
	bzero(&m_SendBuffer, sizeof(m_SendBuffer));
	messageSize = sprintf(m_SendBuffer, "102 MOVE\a\b");
	send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	receivePackage(recvSize, &timeout, receivedPackage);
	
	if (recvSize > 2){
		if (sscanf(receivedPackage, "%2s%d%d", okStr, &x, &y) != 3 || !checkMove(receivedPackage)){
			messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	   	 send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
			return false;

		}
		cout << "( "<< x <<" , " << y << " )" << endl;
		bzero(&receivedPackage, sizeof(receivedPackage));
	}
	else {
	   messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	   send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	   return false;
	}
	//bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
	bzero(&m_SendBuffer, sizeof(m_SendBuffer));
	messageSize = sprintf(m_SendBuffer, "102 MOVE\a\b");
	send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	receivePackage(recvSize, &timeout, receivedPackage);
	if (recvSize > 2){
		if (sscanf(receivedPackage, "%2s%d%d", okStr, &x1, &y1) != 3 
					|| !checkMove(receivedPackage)){
			messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	   	 send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
			return false;

		}
		cout << "( "<< x1 <<" , " << y1 << " )" << endl;
		this->m_Coordinates.setX(x1);
		this->m_Coordinates.setY(y1);
		this->m_Coordinates.setDir(x1-x, y1-y);
		cout << m_Coordinates.getDir() << endl;
		bzero(&receivedPackage, sizeof(receivedPackage));
	}
	else {
	   messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
	   send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
	   return false;
	}
	while (!m_Coordinates.isInSquare()){
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));
		//bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
		bzero(&m_SendBuffer, sizeof(m_SendBuffer));
		if (abs(m_Coordinates.getX()) <= 2 && abs(m_Coordinates.getY()) <= 2) break;
		else if (abs(m_Coordinates.getX()) > 2) {
			if (m_Coordinates.getX() > 2){
				if(!this->goLeft(&timeout)) return false;
			}else {
				if(!this->goRight(&timeout)) return false;
			}
			continue;
		}
		else {
			if (m_Coordinates.getY() > 2){
				if(!this->goDown(&timeout)) return false;;
			}else {
				if(!this->goUp(&timeout)) return false;;
			}
			continue;
		}
		
	}	 
	return true;
	//std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}


int const CRobotClient::neighs[4][2] = {
        { 0,  1},
        { 1,  0},
        { 0, -1},
        {-1,  0}
};

bool CRobotClient::DFS(int i, int j, struct timeval * timeout) 
{
    set <int> visited;
    stack <pair<int, int>> myStack; 
    myStack.push(make_pair(i, j));
    while (!myStack.empty()){
        pair<int, int> cur = myStack.top();
	visited.insert((cur.first+2)*5+(cur.second+2));
        myStack.pop();
	if (!moveTo(cur.first, cur.second, timeout)) return false;
	if (tryPickUp(timeout)) {
		close(m_Socket);
		return true;
	}

    	for (int k = 0; k < 4; k++){
		int i0 = i + neighs[k][0], j0 = j + neighs[k][1];
		if(abs(i0) <= 2 && abs(j0) <= 2 && visited.find((i0+2)*5+(j0+2)) == visited.end())
			myStack.push(make_pair(i0, j0));
	}
    }
    int messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
    send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
    return true;
}


void CRobotClient::pickUpMessage(){
	cout << "Picking up a message, socket: " << m_Socket << endl;
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	DFS(this->m_Coordinates.getX(), this->m_Coordinates.getY(), &timeout);
	
	//std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}


void socketThread(void * dummyArg){
	int slaveSocket = (int) (intptr_t) dummyArg;
	CRobotClient currentRobot(slaveSocket);
	if (!currentRobot.authentication()) {
		close(slaveSocket);
		return;
	}
	if (!currentRobot.moveToGoal()) {
		close(slaveSocket);
		return;
	}
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




