#define _WIN32_WINNT 0x0600
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <limits>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

// Global variables
bool loggedIn = false;
std::string playerName;
std::vector<std::string> sessionList;
int currentSession = -1;

// Function declarations
void clearScreen();
void printMenu();
void displayBoard(const char board[9]);
void handleSessionAccess(const char* buffer);
void requestSessionList(SOCKET sock);
void displaySessionList(const char* data, int length);
void handleServerResponse(SOCKET sock);
void handleMakeMove(SOCKET sock);
void handleClient(SOCKET sock);

// Function definitions
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

    // Wait for server response
    char recvBuffer[512];
    int bytesReceived = recv(sock, recvBuffer, sizeof(recvBuffer), 0);
    if (bytesReceived > 0 && recvBuffer[4] == 103) {  // Assuming 103 is the session list response code
        displaySessionList(recvBuffer + 5, bytesReceived - 5);
    }
    else {
        std::cout << "Failed to receive session list.\n";
    }
}

void displaySessionList(const char* data, int length) {
    sessionList.clear();
    int offset = 0;
    while (offset < length) {
        int sessionNumber = *(int*)(data + offset);
        offset += 4;
        int ownerNameLength = data[offset++];
        std::string ownerName(data + offset, ownerNameLength);
        offset += ownerNameLength;
        bool passwordProtected = data[offset++];

        std::string sessionInfo = "Session ID: " + std::to_string(sessionNumber) +
            " | Owner: " + ownerName +
            (passwordProtected ? " | [Password Protected]" : " | [Open]");
        sessionList.push_back(sessionInfo);
    }

    if (sessionList.empty()) {
        std::cout << "No available sessions.\n";
    }
    else {
        std::cout << "Available Sessions:\n";
        for (const auto& session : sessionList) {
            std::cout << session << std::endl;
        }
    }
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

        // Recalling the menu so the user can choose another action
        handleClient(sock);
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
    std::string input;

    do {
        printMenu();
        std::cout << "Enter your choice: ";
        std::getline(std::cin, input);

        try {
            choice = std::stoi(input);
        }
        catch (const std::exception&) {
            choice = -1;  // Invalid input
        }

        bool invalidInput = false;

        switch (choice) {
        case 1:  // Register
            if (!loggedIn) {
                std::cout << "Enter name: ";
                std::getline(std::cin, playerName);
                std::string password;
                std::cout << "Enter password (max 255 chars, no spaces): ";
                std::getline(std::cin, password);

                if (password.empty() || password.find(' ') != std::string::npos || playerName.empty()) {
                    clearScreen();
                    std::cout << "Invalid input! Password and name cannot be empty or contain spaces.\n";
                    invalidInput = true;
                    break;
                }

                int nameLength = static_cast<int>(playerName.size());
                int passwordLength = static_cast<int>(password.size());
                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + nameLength + 1 + passwordLength;
                buffer[4] = 1;
                buffer[5] = static_cast<char>(nameLength);
                memcpy(buffer + 6, playerName.c_str(), nameLength);
                buffer[6 + nameLength] = static_cast<char>(passwordLength);
                memcpy(buffer + 7 + nameLength, password.c_str(), passwordLength);
                send(sock, buffer, 7 + nameLength + passwordLength, 0);
            }
            if (loggedIn)
            {
                clearScreen();
                std::cout << "Invalid option, please try again.\n";
                continue;
            }
            break;

        case 2:  // Login
            if (!loggedIn) {
                std::cout << "Enter name: ";
                std::getline(std::cin, playerName);
                std::string password;
                std::cout << "Enter password: ";
                std::getline(std::cin, password);

                if (playerName.empty() || password.empty()) {
                    clearScreen();
                    std::cout << "Name and password cannot be empty.\n";
                    invalidInput = true;
                    break;
                }

                int nameLength = static_cast<int>(playerName.size());
                int passwordLength = static_cast<int>(password.size());
                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + nameLength + 1 + passwordLength;
                buffer[4] = 3;
                buffer[5] = static_cast<char>(nameLength);
                memcpy(buffer + 6, playerName.c_str(), nameLength);
                buffer[6 + nameLength] = static_cast<char>(passwordLength);
                memcpy(buffer + 7 + nameLength, password.c_str(), passwordLength);
                send(sock, buffer, 7 + nameLength + passwordLength, 0);
            }
            if (loggedIn)
            {
                clearScreen();
                std::cout << "Invalid option, please try again.\n";
                continue;
            }
            break;

        case 3:  // Request session list
            if (loggedIn) {
                requestSessionList(sock);
                std::cout << "Press Enter to continue...";
                std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                clearScreen();  // Clear the screen after user presses Enter
                continue;  // Skip the rest of the loop and start again from the top
            }
            if (!loggedIn)
            {
                clearScreen();
                std::cout << "Invalid option, please try again.\n";
                continue;
            }
            break;

        case 4:  // Create session
            if (loggedIn) {
                std::string sessionPassword;
                std::cout << "Enter session password (leave empty if no password, max 255 chars, no spaces): ";
                std::getline(std::cin, sessionPassword);

                if (sessionPassword.find(' ') != std::string::npos) {
                    clearScreen();
                    std::cout << "Invalid input! Password cannot contain spaces.\n";
                    invalidInput = true;
                    break;
                }

                int passwordLength = static_cast<int>(sessionPassword.size());

                if (passwordLength > 255) {
                    clearScreen();
                    std::cout << "Password is too long (max 255 characters).\n";
                    invalidInput = true;
                    break;
                }

                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + passwordLength;
                buffer[4] = 5;
                buffer[5] = static_cast<char>(passwordLength);
                memcpy(buffer + 6, sessionPassword.c_str(), passwordLength);
                send(sock, buffer, 6 + passwordLength, 0);
            }
            if (!loggedIn)
            {
                clearScreen();
                std::cout << "Invalid option, please try again.\n";
                continue;
            }
            break;

        case 5:  // Join session
            if (loggedIn) {
                std::string sessionNumberInput;
                std::cout << "Enter session number: ";
                std::getline(std::cin, sessionNumberInput);

                int sessionNumber;
                try {
                    sessionNumber = std::stoi(sessionNumberInput);
                }
                catch (const std::exception&) {
                    clearScreen();
                    std::cout << "Invalid session number.\n";
                    invalidInput = true;
                    continue;
                }

                std::string sessionPassword;
                clearScreen();
                std::cout << "Enter session password (if required): ";
                std::getline(std::cin, sessionPassword);

                if (sessionPassword.find(' ') != std::string::npos) {
                    clearScreen();
                    std::cout << "Invalid input! Password cannot contain spaces.\n";
                    invalidInput = true;
                    break;
                }

                int passwordLength = static_cast<int>(sessionPassword.size());

                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + 1 + passwordLength;
                buffer[4] = 6;
                buffer[5] = static_cast<char>(sessionNumber);
                buffer[6] = static_cast<char>(passwordLength);
                memcpy(buffer + 7, sessionPassword.c_str(), passwordLength);
                send(sock, buffer, 7 + passwordLength, 0);
            }
            if (!loggedIn)
            {
                clearScreen();
                std::cout << "Invalid option, please try again.\n";
                continue;
            }
            break;

        case 6:  // Logout
            if (loggedIn) {
                char buffer[5] = { 0 };
                *(int*)buffer = 1;
                buffer[4] = 4;  // Logout message code
                send(sock, buffer, 5, 0);

                // Warte auf die Server-Antwort
                char response[512];
                int bytesReceived = recv(sock, response, sizeof(response), 0);
                if (bytesReceived > 0 && response[4] == 104) {  // 104 ist der Logout-Bestätigungscode
                    loggedIn = false;
                    sessionList.clear();  // Sitzungsliste leeren
                    currentSession = -1;  // Setze die aktuelle Sitzung zurück
                    clearScreen();
                    std::cout << "Logged out successfully.\n";
                    continue;
                }
                else {
                    clearScreen();
                    std::cout << "Logout failed.\n";
                }
            }
            if (!loggedIn)
            {
                clearScreen();
                std::cout << "Invalid option, please try again.\n";
                continue;
            }

            // Zeige nach dem Logout wieder das Hauptmenü an
            break;  // Zurück zur Hauptschleife (do-while), um das Menü erneut anzuzeigen


        case 7:  // Make move
            if (loggedIn) {
                handleMakeMove(sock);
            }
            if (!loggedIn)
            {
                clearScreen();
                std::cout << "Invalid option, please try again.\n";
                continue;
            }
            break;

        case 0:  // Exit
            if (loggedIn) {
                // Führe Logout durch
                char buffer[5] = { 0 };
                *(int*)buffer = 1;
                buffer[4] = 4;  // Logout message code
                send(sock, buffer, 5, 0);

                // Warte auf die Server-Antwort, um das Logout zu bestätigen
                char response[512];
                int bytesReceived = recv(sock, response, sizeof(response), 0);
                if (bytesReceived > 0 && response[4] == 104) {  // 104 ist der Logout-Bestätigungscode
                    clearScreen();
                    std::cout << "Logged out successfully.\n";
                }
                else {
                    clearScreen();
                    std::cout << "Logout failed.\n";
                }
            }

            // Schließe die Socket-Verbindung
            closesocket(sock);
            WSACleanup();  // Windows-spezifische Netzwerkschicht aufräumen

            std::cout << "Exiting...\n";
            FreeConsole();
            exit(0);  // Beendet das Programm
            


        default:
            clearScreen();
            std::cout << "Invalid option, please try again.\n";
            invalidInput = true;
        }

        if (!invalidInput) {
            // Check for server messages after each action
            handleServerResponse(sock);

            if (choice != 0) {
                // Clear the screen and wait for user input before showing the menu again
                clearScreen();
                std::cout << "Press Enter to continue...";
                std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            }
        }
        else {
            // If there was an invalid input, wait for user acknowledgment before redisplaying the menu
            clearScreen();
            std::cout << "Press Enter to continue...";
            std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        }

    } while (true);
}

int main() {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "Failed to initialize Winsock\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(3400);

    if (inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)) <= 0) {
        std::cerr << "Invalid address\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    handleClient(sock);

    // Cleanup
    closesocket(sock);
    WSACleanup();
    return 0;
}