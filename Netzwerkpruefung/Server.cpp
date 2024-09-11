#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")

const int PORT = 3400;
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
    bool gameStarted = false;
};

struct User {
    std::string name;
    std::string password;
    SOCKET socket;
    bool loggedIn = false;
    int currentSession = -1;
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
    *(int*)message.data() = static_cast<int>(1 + data.size());
    message[4] = messageCode;
    std::copy(data.begin(), data.end(), message.begin() + 5);
    send(clientSocket, message.data(), static_cast<int>(message.size()), 0);
}

void handleRegister(SOCKET clientSocket, const std::vector<char>& message) {
    int nameLength = message[5];
    std::string name(message.begin() + 6, message.begin() + 6 + nameLength);
    int passwordLength = message[6 + nameLength];
    std::string password(message.begin() + 7 + nameLength, message.begin() + 7 + nameLength + passwordLength);

    if (registeredUsers.find(name) == registeredUsers.end()) {
        registeredUsers[name] = { name, password, clientSocket, true, -1 };
        sendMessage(clientSocket, 101);
        std::cout << "[SUCCESS] User " << name << " registered and logged in successfully." << std::endl;
    }
    else {
        sendMessage(clientSocket, 102);
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
        if (it->second.loggedIn) {
            sendMessage(clientSocket, 105);
            std::cout << "[ERROR] User " << name << " is already logged in." << std::endl;
        }
        else {
            it->second.socket = clientSocket;
            it->second.loggedIn = true;
            sendMessage(clientSocket, 104);
            std::cout << "[SUCCESS] User " << name << " logged in successfully." << std::endl;
        }
    }
    else {
        sendMessage(clientSocket, 105);
        std::cout << "[ERROR] Login failed for user " << name << std::endl;
    }
}

void sendGameState(const Session& session, char messageCode) {
    std::vector<char> stateData(14 + session.player2.size(), 0);
    stateData[0] = static_cast<char>(session.sessionNumber);
    stateData[1] = static_cast<char>(session.player2.size());
    std::copy(session.player2.begin(), session.player2.end(), stateData.begin() + 2);
    std::copy(std::begin(session.board), std::end(session.board), stateData.begin() + 2 + session.player2.size());
    stateData[2 + session.player2.size() + 9] = session.isPlayer1Turn ? 1 : 0;

    for (const auto& pair : registeredUsers) {
        if (pair.second.name == session.player1) {
            sendMessage(pair.second.socket, messageCode, stateData);
            break;
        }
    }

    stateData[1] = static_cast<char>(session.player1.size());
    std::copy(session.player1.begin(), session.player1.end(), stateData.begin() + 2);
    stateData[2 + session.player1.size() + 9] = session.isPlayer1Turn ? 0 : 1;

    for (const auto& pair : registeredUsers) {
        if (pair.second.name == session.player2) {
            sendMessage(pair.second.socket, messageCode, stateData);
            break;
        }
    }
}

void handleLogout(SOCKET clientSocket) {
    std::string logoutUserName;
    for (auto& pair : registeredUsers) {
        User& user = pair.second;
        if (user.socket == clientSocket && user.loggedIn) {
            logoutUserName = user.name;
            user.loggedIn = false;
            user.currentSession = -1;
            break;
        }
    }

    if (logoutUserName.empty()) {
        std::cout << "[ERROR] Logout failed: user not found or already logged out." << std::endl;
        return;
    }

    // Check all active sessions
    for (auto it = activeSessions.begin(); it != activeSessions.end();) {
        Session& session = it->second;
        if (session.player1 == logoutUserName || session.player2 == logoutUserName) {
            std::string winnerName = (session.player1 == logoutUserName) ? session.player2 : session.player1;

            // Notify the other player about the win
            for (const auto& userPair : registeredUsers) {
                if (userPair.second.name == winnerName && userPair.second.loggedIn) {
                    session.isPlayer1Turn = (session.player1 == logoutUserName);
                    sendGameState(session, 110);  // Game over
                    std::cout << "[INFO] Player " << winnerName << " wins in session " << session.sessionNumber << " due to " << logoutUserName << " logout." << std::endl;
                    break;
                }
            }

            // Remove the session
            it = activeSessions.erase(it);
        }
        else {
            ++it;
        }
    }

    std::cout << "[INFO] User " << logoutUserName << " logged out successfully." << std::endl;
    sendMessage(clientSocket, 104);  // 104 is the successful logout message code
}
void handleSessionListRequest(SOCKET clientSocket) {
    std::vector<char> sessionListData;
    for (const auto& session : activeSessions) {
        if (session.second.player2.empty()) {  // Only show sessions with one player
            sessionListData.insert(sessionListData.end(), (char*)&session.second.sessionNumber, (char*)&session.second.sessionNumber + 4);

            // Add the owner name length and the owner name
            char ownerNameLength = static_cast<char>(session.second.player1.size());
            sessionListData.push_back(ownerNameLength);
            sessionListData.insert(sessionListData.end(), session.second.player1.begin(), session.second.player1.end());

            // Add password protection flag
            sessionListData.push_back(static_cast<char>(session.second.passwordProtected));
        }
    }
    sendMessage(clientSocket, 103, sessionListData);
    std::cout << "[INFO] Sent session list with " << sessionListData.size() / 6 << " available sessions." << std::endl;
}

void handleCreateSession(SOCKET clientSocket, const std::vector<char>& message) {
    int passwordLength = message[5];
    std::string password(message.begin() + 6, message.begin() + 6 + passwordLength);

    if (activeSessions.size() >= SESSION_LIMIT) {
        std::vector<char> denyData = { 4 };
        sendMessage(clientSocket, 112, denyData);
        std::cout << "[ERROR] Session limit reached, can't create new session." << std::endl;
        return;
    }

    sessionCounter++;
    std::string playerName;

    for (const auto& pair : registeredUsers) {
        const std::string& name = pair.first;
        const User& user = pair.second;

        if (user.socket == clientSocket && user.loggedIn) {
            playerName = name;
            break;
        }
    }

    Session newSession = { sessionCounter, playerName, "", !password.empty(), password };
    activeSessions[sessionCounter] = newSession;

    std::vector<char> accessData(13, 0);
    *(int*)accessData.data() = sessionCounter;
    accessData[4] = 0;
    accessData[5] = 1;
    std::fill(accessData.begin() + 6, accessData.end(), '-');

    sendMessage(clientSocket, 108, accessData);

    std::cout << "[SUCCESS] User " << playerName << " Created new session with ID: " << sessionCounter << std::endl;
}

void handleJoinSession(SOCKET clientSocket, const std::vector<char>& message) {
    int sessionNumber = message[5];
    int passwordLength = message[6];
    std::string sessionPassword(message.begin() + 7, message.begin() + 7 + passwordLength);

    std::string playerName;
    for (const auto& pair : registeredUsers) {
        const User& user = pair.second;
        if (user.socket == clientSocket && user.loggedIn) {
            playerName = user.name;
            break;
        }
    }

    if (playerName.empty()) {
        std::cout << "[ERROR] User not found or not logged in." << std::endl;
        return;
    }

    auto sessionIt = activeSessions.find(sessionNumber);
    if (sessionIt == activeSessions.end()) {
        std::vector<char> denyData = { 0 };
        sendMessage(clientSocket, 112, denyData);
        std::cout << "[ERROR] Session " << sessionNumber << " does not exist." << std::endl;
        return;
    }

    Session& session = sessionIt->second;

    if (!session.player2.empty()) {
        std::vector<char> denyData = { 1 };
        sendMessage(clientSocket, 112, denyData);
        std::cout << "[ERROR] Session " << sessionNumber << " is full." << std::endl;
        return;
    }

    if (session.passwordProtected && session.password != sessionPassword) {
        std::vector<char> denyData = { 2 };
        sendMessage(clientSocket, 112, denyData);
        std::cout << "[ERROR] Incorrect password for session " << sessionNumber << "." << std::endl;
        return;
    }

    if (session.player1 == playerName || session.player2 == playerName) {
        std::vector<char> denyData = { 3 };
        sendMessage(clientSocket, 112, denyData);
        std::cout << "[ERROR] User " << playerName << " is already in session " << sessionNumber << "." << std::endl;
        return;
    }

    session.player2 = playerName;
    session.gameStarted = true;

    std::vector<char> accessData(14 + session.player1.size(), 0);
    accessData[0] = static_cast<char>(sessionNumber);
    accessData[1] = static_cast<char>(session.player1.size());
    std::copy(session.player1.begin(), session.player1.end(), accessData.begin() + 2);
    std::copy(std::begin(session.board), std::end(session.board), accessData.begin() + 2 + session.player1.size());
    accessData[2 + session.player1.size() + 9] = 0;  // isPlayersTurn = false for player 2

    sendMessage(clientSocket, 108, accessData);

    // Notify player 1 that the game has started
    std::vector<char> player1AccessData(14 + session.player2.size(), 0);
    player1AccessData[0] = static_cast<char>(sessionNumber);
    player1AccessData[1] = static_cast<char>(session.player2.size());
    std::copy(session.player2.begin(), session.player2.end(), player1AccessData.begin() + 2);
    std::copy(std::begin(session.board), std::end(session.board), player1AccessData.begin() + 2 + session.player2.size());
    player1AccessData[2 + session.player2.size() + 9] = 1;  // isPlayersTurn = true for player 1

    for (const auto& pair : registeredUsers) {
        if (pair.second.name == session.player1) {
            sendMessage(pair.second.socket, 108, player1AccessData);
            break;
        }
    }

    std::cout << "[SUCCESS] User " << playerName << " joined session " << sessionNumber << "." << std::endl;
}

bool checkWin(const char* board) {
    const int winPatterns[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8},  // Rows
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8},  // Columns
        {0, 4, 8}, {2, 4, 6}              // Diagonals
    };

    for (int i = 0; i < 8; ++i) {
        if (board[winPatterns[i][0]] != 0 &&
            board[winPatterns[i][0]] == board[winPatterns[i][1]] &&
            board[winPatterns[i][0]] == board[winPatterns[i][2]]) {
            return true;
        }
    }
    return false;
}

bool checkDraw(const char* board) {
    return std::all_of(board, board + 9, [](char c) { return c != 0; });
}


void handleMakeMove(SOCKET clientSocket, const std::vector<char>& message) {
    int sessionNumber = *(int*)(message.data() + 5);
    int coordinate = message[9];

    auto sessionIt = activeSessions.find(sessionNumber);
    if (sessionIt == activeSessions.end()) {
        std::cout << "[ERROR] Session " << sessionNumber << " not found." << std::endl;
        return;
    }

    Session& session = sessionIt->second;
    std::string playerName;

    // Find the player making the move
    for (const auto& pair : registeredUsers) {
        if (pair.second.socket == clientSocket) {
            playerName = pair.first;
            break;
        }
    }

    bool isPlayer1 = (playerName == session.player1);
    if ((isPlayer1 && !session.isPlayer1Turn) || (!isPlayer1 && session.isPlayer1Turn)) {
        std::cout << "[ERROR] Not your turn." << std::endl;
        sendGameState(session, 109);  // Update game state
        return;
    }

    // Check if the move is valid
    if (coordinate < 0 || coordinate >= 9 || session.board[coordinate] != 0) {
        std::cout << "[ERROR] Invalid move." << std::endl;
        sendGameState(session, 109);  // Update game state

        return;
    }

    // Update the board
    session.board[coordinate] = isPlayer1 ? 'X' : 'O';
    session.isPlayer1Turn = !session.isPlayer1Turn;

    // Display the updated board on the server side
   // displayBoard(session.board);

    // Check if someone won the game
    if (checkWin(session.board)) {
        sendGameState(session, 110);  // Game over
        std::cout << "[INFO] Player " << playerName << " won the game in session " << sessionNumber << std::endl;
        activeSessions.erase(sessionIt);  // Remove session after game over
    }
    // Check if the game is a draw
    else if (checkDraw(session.board)) {
        sendGameState(session, 111);  // Game draw
        std::cout << "[INFO] Game ended in a draw in session " << sessionNumber << std::endl;
        activeSessions.erase(sessionIt);  // Remove session after draw
    }
    // Continue the game if it's not over
    else {
        sendGameState(session, 109);  // Update game state
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
                    handleLogout(sock);
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

        // Verarbeite die Nachrichten in den Clientpuffern
        for (auto& client : clientBuffers) {
            SOCKET clientSocket = client.first;
            std::vector<char>& buffer = client.second;

            while (buffer.size() >= 5) {
                int messageLength = *(int*)buffer.data();
                if (buffer.size() >= messageLength + 4) {
                    std::vector<char> message(buffer.begin(), buffer.begin() + messageLength + 4);
                    int messageCode = message[4];

                    switch (messageCode) {
                    case 1: handleRegister(clientSocket, message); break;
                    case 2: handleSessionListRequest(clientSocket); break;
                    case 3: handleLogin(clientSocket, message); break;
                    case 4: handleLogout(clientSocket); break;
                    case 5: handleCreateSession(clientSocket, message); break;
                    case 6: handleJoinSession(clientSocket, message); break;
                    case 8: handleMakeMove(clientSocket, message); break;
                    default: std::cout << "[ERROR] Unknown message code: " << messageCode << std::endl;
                    }

                    buffer.erase(buffer.begin(), buffer.begin() + messageLength + 4);
                }
                else {
                    break;
                }
            }
        }
    }

    closesocket(listener);
    WSACleanup();
    return 0;
}