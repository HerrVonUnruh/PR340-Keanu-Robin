#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "Ws2_32.lib")

const int PORT = 54000;
const int BUFFER_SIZE = 512;
const int SESSION_LIMIT = 30;

struct Session {
    int sessionNumber;
    std::string player1;
    std::string player2;
    bool passwordProtected;
    std::string password;
    char board[9] = { 0 };
    bool isPlayer1Turn = true;
};

struct User {
    std::string name;
    std::string password;
    SOCKET socket;
};

std::unordered_map<std::string, User> registeredUsers;
std::unordered_map<int, Session> activeSessions;
std::unordered_map<SOCKET, std::vector<char>> clientBuffers;
int sessionCounter = 0;

void initWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        exit(1);
    }
}

SOCKET createListenerSocket() {
    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) {
        std::cerr << "Can't create a socket." << std::endl;
        WSACleanup();
        exit(1);
    }

    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(PORT);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(listener, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        std::cerr << "Can't bind to IP/port." << std::endl;
        closesocket(listener);
        WSACleanup();
        exit(1);
    }

    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Can't listen." << std::endl;
        closesocket(listener);
        WSACleanup();
        exit(1);
    }

    u_long mode = 1;
    ioctlsocket(listener, FIONBIO, &mode);

    std::cout << "Server started and listening on port " << PORT << std::endl;
    return listener;
}

void sendMessage(SOCKET clientSocket, char messageCode, const std::vector<char>& data = {}) {
    std::vector<char> message(5 + data.size());
    *(int*)message.data() = (int)(1 + data.size());
    message[4] = messageCode;
    std::copy(data.begin(), data.end(), message.begin() + 5);
    send(clientSocket, message.data(), message.size(), 0);
}

void handleRegister(SOCKET clientSocket, const std::vector<char>& message) {
    int nameLength = message[5];
    std::string name(message.begin() + 6, message.begin() + 6 + nameLength);
    int passwordLength = message[6 + nameLength];
    std::string password(message.begin() + 7 + nameLength, message.begin() + 7 + nameLength + passwordLength);

    if (registeredUsers.find(name) == registeredUsers.end()) {
        registeredUsers[name] = { name, password, clientSocket };
        sendMessage(clientSocket, 101);  // confirmRegister
        std::cout << "[SUCCESS] User " << name << " registered successfully." << std::endl;
    }
    else {
        sendMessage(clientSocket, 102);  // denyRegister
        std::cout << "[ERROR] Registration failed, user " << name << " already exists." << std::endl;
    }
}

void handleLogin(SOCKET clientSocket, const std::vector<char>& message) {
    int nameLength = message[5];
    std::string name(message.begin() + 6, message.begin() + 6 + nameLength);
    int passwordLength = message[6 + nameLength];
    std::string password(message.begin() + 7 + nameLength, message.begin() + 7 + nameLength + passwordLength);

    auto it = registeredUsers.find(name);
    if (it != registeredUsers.end() && it->second.password == password) {
        it->second.socket = clientSocket;
        sendMessage(clientSocket, 104);  // confirmLogin
        std::cout << "[SUCCESS] User " << name << " logged in successfully." << std::endl;
    }
    else {
        sendMessage(clientSocket, 105);  // denyLogin
        std::cout << "[ERROR] Login failed for user " << name << std::endl;
    }
}

void handleSessionListRequest(SOCKET clientSocket) {
    std::vector<char> sessionListData;
    for (const auto& session : activeSessions) {
        sessionListData.insert(sessionListData.end(), (char*)&session.second.sessionNumber, (char*)&session.second.sessionNumber + 4);
        sessionListData.push_back(static_cast<char>(session.second.player2.empty() ? 0 : session.second.player2.size()));
        sessionListData.push_back(static_cast<char>(session.second.passwordProtected));
    }
    sendMessage(clientSocket, 103, sessionListData);  // sendSessionList
    std::cout << "[INFO] Sent session list with " << activeSessions.size() << " active sessions." << std::endl;
}

void handleCreateSession(SOCKET clientSocket, const std::vector<char>& message) {
    int passwordLength = message[5];
    std::string password(message.begin() + 6, message.begin() + 6 + passwordLength);

    if (activeSessions.size() >= SESSION_LIMIT) {
        std::vector<char> denyData = { 4 };  // Sessionlimit reached
        sendMessage(clientSocket, 112, denyData);  // denySessionAccess
        std::cout << "[ERROR] Session limit reached, can't create new session." << std::endl;
        return;
    }

    sessionCounter++;
    Session newSession = { sessionCounter, "Player1", "", !password.empty(), password };
    activeSessions[sessionCounter] = newSession;

    std::vector<char> accessData(13, 0);
    *(int*)accessData.data() = sessionCounter;
    accessData[4] = 0;  // enemy name length (0 for now)
    accessData[5] = 1;  // isPlayersTurn
    std::fill(accessData.begin() + 6, accessData.end(), '-');
    sendMessage(clientSocket, 108, accessData);  // grantSessionAccess

    std::cout << "[SUCCESS] Created new session with ID: " << sessionCounter << std::endl;
}

void handleJoinSession(SOCKET clientSocket, const std::vector<char>& message) {
    int sessionNumber = *(int*)(message.data() + 5);
    int passwordLength = message[9];
    std::string password(message.begin() + 10, message.begin() + 10 + passwordLength);

    auto it = activeSessions.find(sessionNumber);
    if (it == activeSessions.end()) {
        std::vector<char> denyData = { 0 };  // game does not exist
        sendMessage(clientSocket, 112, denyData);  // denySessionAccess
        std::cout << "[ERROR] Session " << sessionNumber << " does not exist." << std::endl;
        return;
    }

    Session& session = it->second;
    if (!session.player2.empty()) {
        std::vector<char> denyData = { 1 };  // game is full
        sendMessage(clientSocket, 112, denyData);  // denySessionAccess
        std::cout << "[ERROR] Session " << sessionNumber << " is full." << std::endl;
        return;
    }

    if (session.passwordProtected && session.password != password) {
        std::vector<char> denyData = { 2 };  // wrong password
        sendMessage(clientSocket, 112, denyData);  // denySessionAccess
        std::cout << "[ERROR] Incorrect password for session " << sessionNumber << std::endl;
        return;
    }

    session.player2 = "Player2";
    std::vector<char> accessData(13, 0);
    *(int*)accessData.data() = sessionNumber;
    accessData[4] = 7;  // enemy name length ("Player1")
    std::copy(session.player1.begin(), session.player1.end(), accessData.begin() + 5);
    accessData[12] = 0;  // isPlayersTurn (false for player 2)
    std::fill(accessData.begin() + 13, accessData.end(), '-');
    sendMessage(clientSocket, 108, accessData);  // grantSessionAccess

    std::cout << "[SUCCESS] Player joined session " << sessionNumber << std::endl;
}

void processClientMessage(SOCKET clientSocket, const std::vector<char>& message) {
    int messageCode = message[4];
    switch (messageCode) {
    case 1:
        handleRegister(clientSocket, message);
        break;
    case 3:
        handleLogin(clientSocket, message);
        break;
    case 2:
        handleSessionListRequest(clientSocket);
        break;
    case 5:
        handleCreateSession(clientSocket, message);
        break;
    case 6:
        handleJoinSession(clientSocket, message);
        break;
    default:
        std::cout << "[ERROR] Unknown message code: " << messageCode << std::endl;
    }
}

void handleClientMessages() {
    for (auto& client : clientBuffers) {
        SOCKET clientSocket = client.first;
        std::vector<char>& buffer = client.second;

        while (buffer.size() >= 5) {
            int messageLength = *(int*)buffer.data();
            if (buffer.size() >= messageLength + 4) {
                std::vector<char> message(buffer.begin(), buffer.begin() + messageLength + 4);
                processClientMessage(clientSocket, message);
                buffer.erase(buffer.begin(), buffer.begin() + messageLength + 4);
            }
            else {
                break;
            }
        }
    }
}

int main() {
    initWinsock();
    SOCKET listener = createListenerSocket();

    fd_set master;
    FD_ZERO(&master);
    FD_SET(listener, &master);

    while (true) {
        fd_set copy = master;
        int socketCount = select(0, &copy, nullptr, nullptr, nullptr);

        for (int i = 0; i < socketCount; i++) {
            SOCKET sock = copy.fd_array[i];

            if (sock == listener) {
                SOCKET client = accept(listener, nullptr, nullptr);
                FD_SET(client, &master);
                clientBuffers[client] = std::vector<char>();
                std::cout << "[INFO] New client connected." << std::endl;
            }
            else {
                char buf[BUFFER_SIZE];
                int bytesReceived = recv(sock, buf, BUFFER_SIZE, 0);

                if (bytesReceived <= 0) {
                    closesocket(sock);
                    FD_CLR(sock, &master);
                    clientBuffers.erase(sock);
                    std::cout << "[INFO] Client disconnected." << std::endl;
                }
                else {
                    std::vector<char>& clientBuffer = clientBuffers[sock];
                    clientBuffer.insert(clientBuffer.end(), buf, buf + bytesReceived);
                }
            }
        }

        handleClientMessages();
    }

    FD_CLR(listener, &master);
    closesocket(listener);
    WSACleanup();
    return 0;
}