#define _WIN32_WINNT 0x0600
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#pragma comment(lib, "ws2_32.lib")

bool loggedIn = false;
std::string playerName;
std::vector<std::string> sessionList;
int currentSession = -1;

void clearScreen() {
    system("cls");
}

void printMenu() {
    //clearScreen();
    std::cout << "Tic Tac Toe Game Menu\n";

    if (!loggedIn) {
        std::cout << "1. Register\n";
        std::cout << "2. Login\n";
    }
    else {
        std::cout << "3. Request Session List\n";
        std::cout << "4. Create Session\n";
        std::cout << "5. Join Session\n";
        std::cout << "6. Logout\n";
        std::cout << "7. Make Move\n";

        if (!sessionList.empty()) {
            std::cout << "\nAvailable Sessions:\n";
            for (const auto& session : sessionList) {
                std::cout << session << std::endl;
            }
        }
    }

    std::cout << "0. Exit\n";
}

void displayBoard(const char board[9]) {
    std::cout << "Current Board:" << std::endl;
    for (int i = 0; i < 9; ++i) {
        if (i % 3 == 0 && i != 0) std::cout << "\n-----------\n";
        char cell = board[i] == 0 ? ' ' : board[i];  // Empty spots will be displayed as ' '
        std::cout << " " << cell;
        if (i % 3 != 2) std::cout << " |";
    }
    std::cout << std::endl;
}


void handleSessionAccess(const char* buffer) {
    int sessionNumber = buffer[0];
    int enemyNameLength = buffer[1];
    std::string enemyName(buffer + 2, buffer + 2 + enemyNameLength);

    std::vector<char> board(9);
    for (int i = 0; i < 9; ++i) {
        board[i] = buffer[2 + enemyNameLength + i];
    }

    bool isPlayersTurn = buffer[2 + enemyNameLength + 9];

    clearScreen();
    std::cout << "Session Number: " << sessionNumber << std::endl;
    std::cout << "Game against: " << enemyName << std::endl;

    // Display the current board
    displayBoard(board.data());

    std::cout << (isPlayersTurn ? "It's your turn!" : "Waiting for opponent's move...") << std::endl;

    currentSession = sessionNumber;
}


void requestSessionList(SOCKET sock) {
    char buffer[5] = { 0 };
    *(int*)buffer = 1;
    buffer[4] = 2;
    send(sock, buffer, 5, 0);
}



void handleServerResponse(SOCKET sock) {
    char recvBuffer[512];
    int bytesReceived = recv(sock, recvBuffer, 512, 0);
    if (bytesReceived > 0) {
        char messageCode = recvBuffer[4];
        int offset;  // Deklaration außerhalb des switch
        switch (messageCode) {
        case 101: // Erfolgreiche Registrierung
        case 104: // Erfolgreicher Login
            loggedIn = true;
            std::cout << "Login successful!" << std::endl;
            break;
        case 103: // Session List
            offset = 5;  // Initialisierung innerhalb des case
            sessionList.clear();
            while (offset < bytesReceived) {
                int sessionNumber = *(int*)(recvBuffer + offset);
                offset += 4;
                int enemyNameLength = recvBuffer[offset++];
                bool passwordProtected = recvBuffer[offset++];

                std::string sessionInfo = "Session " + std::to_string(sessionNumber);
                sessionInfo += enemyNameLength == 0 ? " (Waiting for player)" : " (Enemy)";
                sessionInfo += passwordProtected ? " [Password Protected]" : " [Open]";
                sessionList.push_back(sessionInfo);
            }
            break;
        case 108: // Session Access
        case 109: // Update Session
        case 110: // Game Over
        case 111: // Draw
            handleSessionAccess(recvBuffer + 5);
            if (messageCode == 110) {
                std::cout << "Game Over! You " << (recvBuffer[2 + recvBuffer[1] + 9] == 1 ? "won!" : "lost!") << std::endl;
                currentSession = -1;
            }
            else if (messageCode == 111) {
                std::cout << "Game Over! It's a draw!" << std::endl;
                currentSession = -1;
            }
            break;
        case 112: // Session Access Denied
            std::cout << "Error: Unable to access session." << std::endl;
            break;
        }
    }
}

void handleMakeMove(SOCKET sock) {
    if (currentSession == -1) {
        std::cout << "You are not in a session. Join a session first." << std::endl;
        return;
    }

    int move;
    std::cout << "Enter your move (0-8): ";
    std::cin >> move;

    if (move < 0 || move > 8) {
        std::cout << "Invalid move. Please enter a number between 0 and 8." << std::endl;
        return;
    }

    std::vector<char> message(10, 0);
    *(int*)message.data() = 6;  // Message length
    message[4] = 8;  // Message code for makeMove
    *(int*)(message.data() + 5) = currentSession;  // Session number
    message[9] = move;  // Move coordinate

    send(sock, message.data(), static_cast<int>(message.size()), 0);

    // Wait for server response to update the board after the move
    handleServerResponse(sock);
}
void handleClient(SOCKET sock) {
    int choice;

    do {
        printMenu();
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        switch (choice) {
        case 1:  // Register
            if (!loggedIn) {
                std::cout << "Enter name: ";
                std::cin >> playerName;
                std::string password;
                std::cout << "Enter password (max 255 chars, no spaces): ";
                std::cin >> password;

                if (password.empty() || password.find(' ') != std::string::npos || playerName.empty()) {
                    std::cout << "Invalid input! Password and name cannot be empty or contain spaces.\n";
                    break;
                }

                int nameLength = playerName.size();
                int passwordLength = password.size();
                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + nameLength + 1 + passwordLength;
                buffer[4] = 1;
                buffer[5] = static_cast<char>(nameLength);
                memcpy(buffer + 6, playerName.c_str(), nameLength);
                buffer[6 + nameLength] = static_cast<char>(passwordLength);
                memcpy(buffer + 7 + nameLength, password.c_str(), passwordLength);
                send(sock, buffer, 7 + nameLength + passwordLength, 0);
            }
            break;

        case 2:  // Login
            if (!loggedIn) {
                std::cout << "Enter name: ";
                std::cin >> playerName;
                std::string password;
                std::cout << "Enter password: ";
                std::cin >> password;

                if (playerName.empty() || password.empty()) {
                    std::cout << "Name and password cannot be empty.\n";
                    break;
                }

                int nameLength = playerName.size();
                int passwordLength = password.size();
                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + nameLength + 1 + passwordLength;
                buffer[4] = 3;
                buffer[5] = static_cast<char>(nameLength);
                memcpy(buffer + 6, playerName.c_str(), nameLength);
                buffer[6 + nameLength] = static_cast<char>(passwordLength);
                memcpy(buffer + 7 + nameLength, password.c_str(), passwordLength);
                send(sock, buffer, 7 + nameLength + passwordLength, 0);
            }
            break;

        case 3:  // Request session list
            if (loggedIn) {
                requestSessionList(sock);
            }
            break;

        case 4:  // Create session
            if (loggedIn) {
                std::string sessionPassword;
                std::cout << "Enter session password (leave empty if no password, max 255 chars, no spaces): ";
                std::cin >> sessionPassword;

                if (sessionPassword.find(' ') != std::string::npos) {
                    std::cout << "Invalid input! Password cannot contain spaces.\n";
                    break;
                }

                int passwordLength = sessionPassword.size();

                if (passwordLength > 255) {
                    std::cout << "Password is too long (max 255 characters).\n";
                    break;
                }

                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + passwordLength;
                buffer[4] = 5;
                buffer[5] = static_cast<char>(passwordLength);
                memcpy(buffer + 6, sessionPassword.c_str(), passwordLength);
                send(sock, buffer, 6 + passwordLength, 0);
            }
            break;

        case 5:  // Join session
            if (loggedIn) {
                int sessionNumber;
                std::string sessionPassword;
                std::cout << "Enter session number: ";
                std::cin >> sessionNumber;
                std::cout << "Enter session password (if required): ";
                std::cin >> sessionPassword;

                if (sessionPassword.find(' ') != std::string::npos) {
                    std::cout << "Invalid input! Password cannot contain spaces.\n";
                    break;
                }

                int passwordLength = sessionPassword.size();

                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + 1 + passwordLength;
                buffer[4] = 6;
                buffer[5] = sessionNumber;
                buffer[6] = passwordLength;
                memcpy(buffer + 7, sessionPassword.c_str(), passwordLength);
                send(sock, buffer, 7 + passwordLength, 0);
            }
            break;

        case 6:  // Logout
            if (loggedIn) {
                char buffer[5] = { 0 };
                *(int*)buffer = 1;
                buffer[4] = 4;
                send(sock, buffer, 5, 0);

                loggedIn = false;
                sessionList.clear();
                currentSession = -1;
                std::cout << "Logged out.\n";
            }
            break;

        case 7:  // Make move
            if (loggedIn) {
                handleMakeMove(sock);
            }
            break;

        case 0:  // Exit
            std::cout << "Exiting...\n";
            break;

        default:
            std::cout << "Invalid option, please try again.\n";
        }

        // Check for server messages after each action
        handleServerResponse(sock);

    } while (choice != 0);
}

int main() {
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(3400);

    inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr));

    connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // Setzen Sie den Socket auf nicht-blockierend
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    handleClient(sock);

    closesocket(sock);
    WSACleanup();
    return 0;
}