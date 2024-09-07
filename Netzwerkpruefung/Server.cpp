#define WIN32_LEAN_AND_MEAN
#include <vector>
#include <map>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>

#pragma comment(lib, "Ws2_32.lib")

std::mutex sessionMutex;
std::atomic<int> sessionIdCounter(1);

// In-Memory Benutzer und Session-Daten
std::map<std::string, std::string> users;

struct Session {
    int sessionId;
    std::string player1;
    std::string player2;
    bool isFull = false;
    char board[3][3];
    char currentTurn = 'X';

    Session(const std::string& p1) : player1(p1), sessionId(sessionIdCounter++) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; j++) {
                board[i][j] = ' ';
            }
        }
    }
};

std::vector<Session> sessions;

bool registerUser(const std::string& username, const std::string& password) {
    if (users.find(username) != users.end()) return false;
    users[username] = password;
    return true;
}

bool loginUser(const std::string& username, const std::string& password) {
    return (users.find(username) != users.end() && users[username] == password);
}

int createSession(const std::string& player1) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    Session newSession(player1);
    sessions.push_back(newSession);
    std::cout << "Session erstellt: ID=" << newSession.sessionId << "\n";
    return newSession.sessionId;
}

bool joinSession(int sessionId, const std::string& player2) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    for (Session& session : sessions) {
        if (session.sessionId == sessionId && !session.isFull) {
            session.player2 = player2;
            session.isFull = true;
            std::cout << player2 << " ist der Session " << sessionId << " beigetreten.\n";
            return true;
        }
    }
    return false;
}

bool checkWin(char board[3][3], char player) {
    for (int i = 0; i < 3; i++) {
        if (board[i][0] == player && board[i][1] == player && board[i][2] == player) return true;
    }
    for (int i = 0; i < 3; i++) {
        if (board[0][i] == player && board[1][i] == player && board[2][i] == player) return true;
    }
    if (board[0][0] == player && board[1][1] == player && board[2][2] == player) return true;
    if (board[0][2] == player && board[1][1] == player && board[2][0] == player) return true;
    return false;
}

bool checkDraw(char board[3][3]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (board[i][j] == ' ') return false;
        }
    }
    return true;
}

std::string processMove(Session& session, int row, int col, char player) {
    if (session.board[row][col] == ' ') {
        session.board[row][col] = player;
        if (checkWin(session.board, player)) return "WIN:" + std::string(1, player);
        if (checkDraw(session.board)) return "DRAW";
        session.currentTurn = (player == 'X') ? 'O' : 'X';
        return "CONTINUE";
    }
    else {
        return "INVALID_MOVE";
    }
}

std::string listSessions() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    std::string sessionList;
    for (const Session& session : sessions) {
        if (!session.isFull) {
            sessionList += "Session ID: " + std::to_string(session.sessionId) + " | Spieler 1: " + session.player1 + "\n";
        }
    }
    if (sessionList.empty()) sessionList = "Keine offenen Sessions.\n";
    return sessionList;
}

void processClientMessage(SOCKET& clientSocket, const std::string& message, const std::string& username) {
    std::cout << "Client Nachricht: " << message << std::endl;

    if (message.substr(0, 9) == "REGISTER:") {
        size_t colon = message.find(':', 9);
        std::string user = message.substr(9, colon - 9);
        std::string password = message.substr(colon + 1);
        std::string response = registerUser(user, password) ? "REGISTRATION_SUCCESS" : "REGISTRATION_FAILED";
        send(clientSocket, response.c_str(), response.size() + 1, 0);
    }
    else if (message.substr(0, 6) == "LOGIN:") {
        size_t colon = message.find(':', 6);
        std::string user = message.substr(6, colon - 6);
        std::string password = message.substr(colon + 1);
        std::string response = loginUser(user, password) ? "LOGIN_SUCCESS" : "LOGIN_FAILED";
        send(clientSocket, response.c_str(), response.size() + 1, 0);
    }
    else if (message == "CREATE_SESSION") {
        int sessionId = createSession(username);
        std::string response = "SESSION_CREATED:ID=" + std::to_string(sessionId);
        send(clientSocket, response.c_str(), response.size() + 1, 0);
    }
    else if (message.substr(0, 12) == "JOIN_SESSION") {
        int sessionId = std::stoi(message.substr(13));
        std::string response = joinSession(sessionId, username) ? "JOIN_SUCCESS" : "JOIN_FAILED";
        send(clientSocket, response.c_str(), response.size() + 1, 0);
    }
    else if (message == "LIST_SESSIONS") {
        std::string sessionList = listSessions();
        send(clientSocket, sessionList.c_str(), sessionList.size() + 1, 0);
    }
    else if (message.substr(0, 4) == "MOVE") {
        size_t firstColon = message.find(':', 5);
        size_t secondColon = message.find(':', firstColon + 1);
        size_t thirdColon = message.find(':', secondColon + 1);

        int sessionId = std::stoi(message.substr(5, firstColon - 5));
        int row = std::stoi(message.substr(firstColon + 1, secondColon - firstColon - 1));
        int col = std::stoi(message.substr(secondColon + 1, thirdColon - secondColon - 1));

        for (Session& session : sessions) {
            if (session.sessionId == sessionId && session.currentTurn == username[0]) {
                std::string result = processMove(session, row, col, session.currentTurn);
                send(clientSocket, result.c_str(), result.size() + 1, 0);
                break;
            }
        }
    }
}

void handleClient(SOCKET clientSocket, const std::string& username) {
    char buffer[512];
    ZeroMemory(buffer, 512);
    while (true) {
        int iResult = recv(clientSocket, buffer, 512, 0);
        if (iResult > 0) {
            processClientMessage(clientSocket, buffer, username);
        }
        else {
            closesocket(clientSocket);
            break;
        }
    }
}

int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverHint;
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(54000);
    serverHint.sin_addr.s_addr = INADDR_ANY;

    iResult = bind(listenSocket, (sockaddr*)&serverHint, sizeof(serverHint));
    if (iResult == SOCKET_ERROR) {
        std::cout << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    iResult = listen(listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        std::cout << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server läuft und wartet auf Verbindungen..." << std::endl;

    while (true) {
        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cout << "Client connection failed: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }

        std::string username = "User" + std::to_string(rand() % 1000);
        std::thread clientThread(handleClient, clientSocket, username);
        clientThread.detach();
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
