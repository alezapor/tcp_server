#include <cstdlib>
#include <cstdio>
#include <sys/socket.h> // socket(), bind(), connect(), listen()
#include <unistd.h> // close(), read(), write()
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // htons(), htonl()
#include <strings.h> // bzero()
#include <chrono>
#include <cstring>
#include <thread>
#include <iostream>
#include <stack>
#include <set>

using namespace std;
#define BUFFER_SIZE 1024
#define SERVER_CODE 54621
#define CLIENT_CODE 45328

char *subString(char *str1) {
    for (int i = 0; i < BUFFER_SIZE - 1; i++) {
        if (str1[i] == '\a' && str1[i + 1] == '\b') return str1 + i + 2;
    }
    return NULL;
}

enum Direction {
    LEFT, UP, RIGHT, DOWN
};

class CCoordinates {

public:
    CCoordinates() : x(0), y(0), dir(LEFT) {}

    void setX(int x) { this->x = x; }

    void setY(int y) { this->y = y; }

    void setDir(int x, int y) {
        if (x == 0) {
            if (y > 0) this->dir = UP;
            else this->dir = DOWN;
        } else {
            if (x > 0) this->dir = RIGHT;
            else this->dir = LEFT;
        }
    }

    int getX() { return x; }

    int getY() { return y; }

    int getDir() { return dir; }

    void move() {
        switch (dir) {
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
                y--;
                break;

        }
    }

    void turnLeft() {
        this->dir = (Direction) ((this->dir + 3) % 4);
    }

    void turnRight() {
        this->dir = (Direction) ((this->dir + 1) % 4);
    }

    bool isInSquare() {

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

    CRobotClient(int socket) : m_Socket(socket), m_BufferTop(0), m_Coordinates(CCoordinates()) {
        m_Timeout.tv_sec = 1;
        m_Timeout.tv_usec = 0;
    }

    bool checkMove(int &, int &);

    bool authentication();

    bool moveToGoal();

    void pickUpMessage();

    bool moveTo(int, int);

    bool recharging();

    bool tryPickUp();

    bool goLeft();

    bool goRight();

    bool goUp();

    bool goDown();

    bool move();  // Move forward

    bool turnLeft();

    bool turnRight();

    bool DFS(int i, int j);

    int receivePackage(int &, int, bool);

private:
    int m_Socket;
    int m_BufferTop;
    char m_SendBuffer[BUFFER_SIZE];
    char m_RecvBuffer[BUFFER_SIZE];
    char m_RecvPackage[BUFFER_SIZE];
    struct timeval m_Timeout;
    CCoordinates m_Coordinates;
};

bool CRobotClient::recharging() {
    /* Initializing variables, setting timeout for recharging */
    int messageSize;
    int recvSize = 0;
    m_Timeout.tv_sec = 5;
    setsockopt(m_Socket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &m_Timeout, sizeof m_Timeout);
    if (receivePackage(recvSize, 12, true) != 0) {
        messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
        return false;

    }
    if (recvSize > 2 && memcmp(m_RecvPackage, "FULL POWER\a\b", 12) != 0) {
        bzero(&m_SendBuffer, sizeof(m_SendBuffer));
        messageSize = sprintf(m_SendBuffer, "302 LOGIC ERROR\a\b");
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
        close(m_Socket);
        return false;
    }
    if (recvSize == 12 && memcmp(m_RecvPackage, "FULL POWER\a\b", 12) == 0) {
        //RECEIVED FULL POWER MSG
        //Change the timeout for receiving general commands
        m_Timeout.tv_sec = 1;
        setsockopt(m_Socket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &m_Timeout, sizeof m_Timeout);
        return true;
    }
    return false;
}

int CRobotClient::receivePackage(int &recvSize, int maxSize = 12, bool reCharging = false) {
    char *subStr;
    while (true) {
        fd_set set;
        int retval;
        subStr = subString(m_RecvBuffer);
        if (subStr != NULL) {
            recvSize = subStr - m_RecvBuffer;
            memcpy(m_RecvPackage, m_RecvBuffer,
                   recvSize);
            memmove(m_RecvBuffer, subStr, BUFFER_SIZE - recvSize);
            m_BufferTop -= recvSize;
            if (memcmp(m_RecvPackage, "RECHARGING\a\b", 12) == 0) {
                if (recharging()) {
                    return receivePackage(recvSize, maxSize);
                } else return -7;
            }
            break;
        }
        FD_ZERO(&set);
        FD_SET(m_Socket, &set);
        if (reCharging) m_Timeout.tv_sec = 5;
        else m_Timeout.tv_sec = 1;
        retval = select(m_Socket + 1, &set, NULL, NULL, &m_Timeout);
        if (retval < 0) {
            perror("Chyba v select()");
            close(m_Socket);
            return -1;
        }
        if (!FD_ISSET(m_Socket, &set)) {
            cout << "Connection timeout!" << endl;
            close(m_Socket);
            return 0;
        }
        unsigned int counter = maxSize;
        while (counter > 0) {
            int res = recv(m_Socket, m_RecvBuffer + m_BufferTop + maxSize - counter, counter, 0);
            if (res > 0) counter -= res;
            if (res == -1) {
                close(m_Socket);
                return -5;

            }
            subStr = subString(m_RecvBuffer);

            if (subStr != NULL) {
                recvSize = subStr - m_RecvBuffer;
                memcpy(m_RecvPackage, m_RecvBuffer,
                       recvSize);
                memmove(m_RecvBuffer, subStr, BUFFER_SIZE - recvSize);
                m_BufferTop -= recvSize;
                break;
            }
        }
        if (counter == 0 && subStr == NULL) return -6;
        m_BufferTop += maxSize - counter;
        if (memcmp(m_RecvPackage, "RECHARGING\a\b", 12) == 0) {
            if (recharging()) {
                return receivePackage(recvSize, maxSize);
            } else return -7;
        }
        if (counter == (unsigned) maxSize) {
            perror("Chyba pri cteni ze socketu.");
            close(m_Socket);
            return -3;
        }
        break;
    }
    return 0;
}

bool CRobotClient::goUp() {
    switch (m_Coordinates.getDir()) {
        case UP:
            if (!move()) return false;
            break;
        case RIGHT:
        case DOWN:
            if (!turnLeft()) return false;;
            break;
        default:
            if (!turnRight()) return false;;
    }
    return true;
}

bool CRobotClient::goDown() {
    switch (m_Coordinates.getDir()) {
        case DOWN:
            if (!move()) return false;
            break;
        case LEFT:
        case UP:
            if (!turnLeft()) return false;;
            break;
        default:
            if (!turnRight()) return false;;
    }
    return true;
}

bool CRobotClient::goLeft() {
    switch (m_Coordinates.getDir()) {
        case LEFT:
            if (!move()) return false;
            break;
        case RIGHT:
        case UP:
            if (!turnLeft()) return false;;
            break;
        default:
            if (!turnRight()) return false;;
    }
    return true;
}

bool CRobotClient::goRight() {
    switch (m_Coordinates.getDir()) {
        case RIGHT:
            if (!move()) return false;
            break;
        case LEFT:
        case UP:
            if (!turnRight()) return false;;
            break;
        default:
            if (!turnLeft()) return false;;
    }
    return true;
}

bool CRobotClient::tryPickUp() {
    int messageSize, recvSize = 0;
    bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
    bzero(&m_SendBuffer, sizeof(m_SendBuffer));
    messageSize = sprintf(m_SendBuffer, "105 GET MESSAGE\a\b");
    send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
    if (receivePackage(recvSize, 100) == -6) {//Timeout, socket errors
        messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
        return true;

    }
    if (recvSize > 2) { //Non-empty message -> loging out
        bzero(&m_SendBuffer, sizeof(m_SendBuffer));
        messageSize = sprintf(m_SendBuffer, "106 LOGOUT\a\b");
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);

        return true;
    }
    //Empty message picked up
    return false;
}

bool CRobotClient::checkMove(int &x, int &y) {
    x = 0;
    y = 0;
    bool minus = false;
    if (strncmp(m_RecvPackage, "OK ", 3) != 0) return false;
    size_t pos = 3;
    if (m_RecvPackage[pos] == '-') {
        minus = true;
        pos++;
    }
    while (m_RecvPackage[pos] != ' ') {
        if (pos > 7) return false;
        else if (m_RecvPackage[pos] >= '0' && m_RecvPackage[pos] <= '9') {
            x = x * 10 + m_RecvPackage[pos] - '0';
            pos++;
        } else return false;
    }
    if (minus) x = -x;
    minus = false;
    pos++;
    if (m_RecvPackage[pos] == '-') {
        minus = true;
        pos++;
    }
    while (m_RecvPackage[pos] != '\a') {
        if (pos > 10) return false;
        else if (m_RecvPackage[pos] >= '0' && m_RecvPackage[pos] <= '9') {
            y = y * 10 + m_RecvPackage[pos] - '0';
            pos++;
        } else return false;
    }
    if (minus) y = -y;
    if (m_RecvPackage[pos + 1] != '\b') return false;
    return true;
}

bool CRobotClient::move() {
    int x1, y1;
    int recvSize, messageSize;
    messageSize = sprintf(m_SendBuffer, "102 MOVE\a\b");
    while (true) {
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
        if (receivePackage(recvSize) != 0) return false; //Timeout, socket error
        if (recvSize > 2) {
            if (!checkMove(x1, y1)) {
                messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
                send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
                return false;
            }
            if (x1 != this->m_Coordinates.getX() + neighs[(int) this->m_Coordinates.getDir()][0]
                || y1 != this->m_Coordinates.getY() + neighs[(int) this->m_Coordinates.getDir()][1])
                continue; //IF robot remains on the same place, try move command again
            this->m_Coordinates.move();
            //bzero(&m_RecvPackage, sizeof(m_RecvPackage));
        } else { //Wrong format
            messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
            send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
            return false;
        }
        return true;
    }
}

bool CRobotClient::moveTo(int i, int j) {
    while (true) {
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
        bzero(&m_SendBuffer, sizeof(m_SendBuffer));
        if (m_Coordinates.getX() == i && m_Coordinates.getY() == j) break;
        else if (m_Coordinates.getX() != i) {
            if (m_Coordinates.getX() > i) {
                if (!this->goLeft()) return false;
            } else {
                if (!this->goRight()) return false;
            }
            continue;
        } else {
            if (m_Coordinates.getY() > j) {
                if (!this->goDown()) return false;
            } else {
                if (!this->goUp()) return false;
            }
            continue;
        }

    }
    return true;
}

bool CRobotClient::turnLeft() {
    int recvSize, messageSize;
    messageSize = sprintf(m_SendBuffer, "103 TURN LEFT\a\b");
    send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
    if (receivePackage(recvSize) != 0) return false;
    this->m_Coordinates.turnLeft();
    return true;
}

bool CRobotClient::turnRight() {
    int recvSize, messageSize;
    messageSize = sprintf(m_SendBuffer, "104 TURN RIGHT\a\b");
    send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
    if (receivePackage(recvSize) != 0) return false;
    this->m_Coordinates.turnRight();
    return true;
}

bool CRobotClient::authentication() {
    /*Initializing local variables, setting the timeout */
    int recvSize = 0, hashCode = 0, clientCode = 0, serverCode = 0;
    int messageSize = 0;
    //receiving the username
    bzero(&m_RecvBuffer, sizeof(m_RecvBuffer));
    bzero(&m_SendBuffer, sizeof(m_SendBuffer));
    setsockopt(m_Socket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &m_Timeout, sizeof m_Timeout);


    /* Receiving the username */
    if (receivePackage(recvSize) != 0) {
        messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
        send(m_Socket, m_SendBuffer, static_cast<size_t>(messageSize), MSG_NOSIGNAL);
        return false;
    }
    if (recvSize > 2) {
        // Computing hashCode
        for (int i = 0; i < recvSize - 2; ++i) {
            hashCode += m_RecvPackage[i];
        }
        hashCode *= 1000;
        hashCode %= 65536;
        serverCode = (SERVER_CODE + hashCode) % 65536;
        messageSize = sprintf(m_SendBuffer, "%d\a\b", serverCode);
        send(m_Socket, m_SendBuffer, static_cast<size_t>(messageSize), MSG_NOSIGNAL);

    } else { // Wrong message format
        messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
        send(m_Socket, m_SendBuffer, static_cast<size_t>(messageSize), MSG_NOSIGNAL);
        return false;
    }

    /* Receiving client's code */
    bzero(&m_SendBuffer, sizeof(m_SendBuffer));
    if (receivePackage(recvSize, 12) != 0) {
        messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
        return false;
    }
    if (recvSize > 2 && recvSize <= 7) {
        for (int i = 0; i < recvSize - 2; i++) {
            if (m_RecvPackage[i] < '0' || m_RecvPackage[i] > '9') { //Wrong code
                messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
                send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
                return false;
            }
            clientCode = clientCode * 10
                         + m_RecvPackage[i] - '0';
        }
        //Checking client's code
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

    } else { //Wrong format
        messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
        return false;

    }
    return true;
}

bool CRobotClient::moveToGoal() {
    int x, y, x1, y1, messageSize = 0, recvSize = 0;

    /* Try to move to get the coordinates */

    bzero(&m_SendBuffer, sizeof(m_SendBuffer));
    messageSize = sprintf(m_SendBuffer, "102 MOVE\a\b");
    send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
    if (receivePackage(recvSize) != 0) {
        messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
        return false;
    }
    if (recvSize > 2) {
        if (!checkMove(x, y)) {
            messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
            send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
            return false;

        }
    } else { //Wrong format - unknown communication message
        messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
        return false;
    }
    bzero(&m_SendBuffer, sizeof(m_SendBuffer));

    /* Try to move once more to get the direction */

    messageSize = sprintf(m_SendBuffer, "102 MOVE\a\b");
    while (true) {
        send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
        if (receivePackage(recvSize) != 0) {
            messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
            send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
            return false;
        }
        if (recvSize > 2) {
            if (!checkMove(x1, y1)) {
                    //Wrong format - OK_<int>_<int>\a\b not recognized
                messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
                send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
                return false;

            }
            this->m_Coordinates.setX(x1);
            this->m_Coordinates.setY(y1);
            this->m_Coordinates.setDir(x1 - x, y1 - y);
            //If the robot does not react to move command, wait until it reacts
            if (x1 != x || y1 != y) break;
        } else {
            messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
            send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
            return false;
        }
    }

    /* Reaching the receiving zone */

    while (true) {
        bzero(&m_SendBuffer, sizeof(m_SendBuffer));
        if (m_Coordinates.isInSquare()) break;
            //Need to change X coordinate
        else if (abs(m_Coordinates.getX()) > 2) {
            if (m_Coordinates.getX() > 2) {
                if (!this->goLeft()) return false;
            } else {
                if (!this->goRight()) return false;
            }
            continue;
        } else { //Need to change Y coordinate
            if (m_Coordinates.getY() > 2) {
                if (!this->goDown()) return false;;
            } else {
                if (!this->goUp()) return false;;
            }
            continue;
        }

    }
    return true;
}


int const 	CRobotClient::neighs[4][2]={
        {-1,  0}, 	// LEFT	 move
        { 0,  1},	// RIGHT move
        { 1,  0},	// UP	 move
        { 0, -1}	// DOWN	 move
};

bool CRobotClient::DFS(int i, int j) {
    set<int> visited;
    stack<pair<int, int>> myStack;
    myStack.push(make_pair(i, j));
    while (!myStack.empty()) {
        pair<int, int> cur = myStack.top();
        myStack.pop();
        if (visited.find((cur.first + 2) * 5 + (cur.second + 2)) == visited.end()) {
            visited.insert((cur.first + 2) * 5 + (cur.second + 2));
            if (!moveTo(cur.first, cur.second)) return false;
            if (tryPickUp()) {
                close(m_Socket);
                return true;
            }

            for (int k = 0; k < 4; k++) { //Search all adjacent vertices
                int i0 = cur.first + neighs[k][0], j0 = cur.second + neighs[k][1];
                if (abs(i0) < 3 && abs(j0) < 3 && visited.find((i0 + 2) * 5 + (j0 + 2)) == visited.end())
                    myStack.push(make_pair(i0, j0));
            }
        }
    }
    //No message found
    int messageSize = sprintf(m_SendBuffer, "301 SYNTAX ERROR\a\b");
    send(m_Socket, m_SendBuffer, messageSize, MSG_NOSIGNAL);
    return true;
}


void CRobotClient::pickUpMessage() {
    //Calling DFS algorithm to check every square in the area
    DFS(this->m_Coordinates.getX(), this->m_Coordinates.getY());
}

//Creating a robot thread
void socketThread(void *dummyArg) {
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

int main(int argc, char **argv) {

    if (argc < 2) {
        cerr << "Usage: server port" << endl;
        return -1;
    }

    // Create master socket
    int masterSocket = socket(AF_INET, /*IPv4*/
                              SOCK_STREAM,
                              IPPROTO_TCP);
    if (masterSocket < 0) {
        perror("Unable to create a master socket");
        return -1;
    }
    int port = atoi(argv[1]);
    if (port == 0) {
        cerr << "Usage: server port" << endl;
        close(masterSocket);
        return -1;
    }
    struct sockaddr_in sockAddr;
    bzero(&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(port);
    sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(masterSocket, (struct sockaddr *) &sockAddr,
             sizeof(sockAddr)) < 0) {
        perror("Error bind()");
        close(masterSocket);
        return -1;
    }

    if (listen(masterSocket, SOMAXCONN) < 0) {
        perror("Error listen()");
        close(masterSocket);
        return -1;
    }



    while (true) {
        int slaveSocket = accept(masterSocket, 0, 0);
        if (slaveSocket < 0) {
            perror("Unable to accept a slave socket");
            return -1;
        }

        thread currentThread(socketThread, (void *) (intptr_t) slaveSocket);
        currentThread.detach();
    }

    close(masterSocket);
    return 0;

}

