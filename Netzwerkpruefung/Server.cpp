#include <iostream>
#include <winsock2.h>
#include <unordered_map>
#include <vector>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

const int PORT = 54000;
const int BUFFER_SIZE = 512;
const int SESSION_LIMIT = 30;

// Session structure
struct Session {
    int sessionNumber;
    std::string player1;
    std::string player2;
    bool passwordProtected;
    std::string password;
    char board[9] = { 0 };
    bool isPlayer1Turn = true;
};

// User structure
struct User {
    std::string name;
    std::string password;
    SOCKET socket;
};

std::unordered_map<std::string, User> registeredUsers;
std::unordered_map<int, Session> activeSessions;
int sessionCounter = 0;

void initWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        exit(1);
    }
}

SOCKET createServerSocket() {
    SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == INVALID_SOCKET) {
        std::cerr << "Can't create a socket." << std::endl;
        WSACleanup();
        exit(1);
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listening, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(listening);
        WSACleanup();
        exit(1);
    }

    if (listen(listening, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(listening);
        WSACleanup();
        exit(1);
    }

    std::cout << "Server started and listening on port " << PORT << std::endl;
    return listening;
}

void handleRegister(SOCKET clientSocket, char* buffer) {
    int nameLength = buffer[5];
    std::string name(buffer + 6, nameLength);
    int passwordLength = buffer[6 + nameLength];
    std::string password(buffer + 7 + nameLength, passwordLength);

    std::cout << "[INFO] Registering user: " << name << std::endl;

    if (registeredUsers.find(name) == registeredUsers.end()) {
        registeredUsers[name] = { name, password, clientSocket };
        std::cout << "[SUCCESS] User " << name << " registered successfully." << std::endl;

        // Send confirm register
        char response[5] = { 0 };
        *(int*)response = 1;
        response[4] = 101;
        send(clientSocket, response, 5, 0);
    }
    else {
        std::cout << "[ERROR] Registration failed, user " << name << " already exists." << std::endl;

        // Send deny register
        char response[5] = { 0 };
        *(int*)response = 1;
        response[4] = 102;
        send(clientSocket, response, 5, 0);
    }
}

void handleLogin(SOCKET clientSocket, char* buffer) {
    int nameLength = buffer[5];
    std::string name(buffer + 6, nameLength);
    int passwordLength = buffer[6 + nameLength];
    std::string password(buffer + 7 + nameLength, passwordLength);

    std::cout << "[INFO] Login attempt for user: " << name << std::endl;

    auto it = registeredUsers.find(name);
    if (it != registeredUsers.end() && it->second.password == password) {
        std::cout << "[SUCCESS] User " << name << " logged in successfully." << std::endl;

        // Send confirm login
        char response[5] = { 0 };
        *(int*)response = 1;
        response[4] = 104;
        send(clientSocket, response, 5, 0);
    }
    else {
        std::cout << "[ERROR] Login failed for user " << name << std::endl;

        // Send deny login
        char response[5] = { 0 };
        *(int*)response = 1;
        response[4] = 105;
        send(clientSocket, response, 5, 0);
    }
}

void handleSessionListRequest(SOCKET clientSocket) {
    std::cout << "[INFO] Session list requested." << std::endl;

    char buffer[BUFFER_SIZE] = { 0 };
    int offset = 5;
    *(int*)buffer = 1;  // Will update the length later
    buffer[4] = 103;  // Send session list message

    for (const auto& session : activeSessions) {
        *(int*)(buffer + offset) = session.second.sessionNumber;
        offset += 4;
        buffer[offset++] = static_cast<char>(session.second.player2.empty() ? 0 : session.second.player2.size());
        buffer[offset++] = static_cast<char>(session.second.passwordProtected);
    }

    int messageLength = offset - 4;
    *(int*)buffer = messageLength;

    send(clientSocket, buffer, messageLength + 4, 0);
    std::cout << "[INFO] Sent session list with " << activeSessions.size() << " active sessions." << std::endl;
}

void handleCreateSession(SOCKET clientSocket, char* buffer) {
    int passwordLength = buffer[5];
    std::string password(buffer + 6, passwordLength);

    if (activeSessions.size() >= SESSION_LIMIT) {
        std::cout << "[ERROR] Session limit reached, can't create new session." << std::endl;
        return;
    }

    sessionCounter++;
    Session newSession = { sessionCounter, "Player1", "", !password.empty(), password };
    activeSessions[sessionCounter] = newSession;

    std::cout << "[SUCCESS] Created new session with ID: " << sessionCounter << std::endl;
}

void handleJoinSession(SOCKET clientSocket, char* buffer) {
    int sessionNumber = *(int*)(buffer + 5);
    int passwordLength = buffer[9];
    std::string password(buffer + 10, passwordLength);

    std::cout << "[INFO] User attempting to join session " << sessionNumber << std::endl;

    auto it = activeSessions.find(sessionNumber);
    if (it != activeSessions.end()) {
        Session& session = it->second;
        if (session.player2.empty()) {
            if (session.passwordProtected && session.password != password) {
                std::cout << "[ERROR] Incorrect password for session " << sessionNumber << std::endl;
                return;
            }
            session.player2 = "Player2";
            std::cout << "[SUCCESS] Player joined session " << sessionNumber << std::endl;
        }
        else {
            std::cout << "[ERROR] Session " << sessionNumber << " is full." << std::endl;
        }
    }
    else {
        std::cout << "[ERROR] Session " << sessionNumber << " does not exist." << std::endl;
    }
}

void processClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];

    while (true) {
        ZeroMemory(buffer, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived <= 0) {
            std::cout << "[INFO] Client disconnected." << std::endl;
            break;
        }

        int messageCode = buffer[4];
        switch (messageCode) {
        case 1:
            handleRegister(clientSocket, buffer);
            break;
        case 3:
            handleLogin(clientSocket, buffer);
            break;
        case 2:
            handleSessionListRequest(clientSocket);
            break;
        case 5:
            handleCreateSession(clientSocket, buffer);
            break;
        case 6:
            handleJoinSession(clientSocket, buffer);
            break;
        default:
            std::cout << "[ERROR] Unknown message code: " << messageCode << std::endl;
        }
    }

    closesocket(clientSocket);
}

int main() {
    initWinsock();

    SOCKET listening = createServerSocket();
    sockaddr_in clientAddr;
    int clientSize = sizeof(clientAddr);

    while (true) {
        SOCKET clientSocket = accept(listening, (sockaddr*)&clientAddr, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "[ERROR] Failed to accept client connection." << std::endl;
            continue;
        }

        std::cout << "[INFO] Client connected." << std::endl;
        processClient(clientSocket);
    }

    closesocket(listening);
    WSACleanup();
    return 0;
}
