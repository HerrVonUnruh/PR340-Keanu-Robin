#define _WIN32_WINNT 0x0600  // Windows Vista oder höher

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#pragma comment(lib, "ws2_32.lib")

bool loggedIn = false;
std::string playerName;
std::vector<std::string> sessionList;

// Funktion zum Löschen des Bildschirms
void clearScreen() {
    system("cls");  // Für Windows
}

void printMenu() {
    clearScreen();
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

        if (!sessionList.empty()) {
            std::cout << "\nAvailable Sessions:\n";
            for (const auto& session : sessionList) {
                std::cout << session << std::endl;
            }
        }
    }

    std::cout << "0. Exit\n";
}

void requestSessionList(SOCKET sock) {
    char buffer[5] = { 0 };
    *(int*)buffer = 1;  // Nachrichtlänge
    buffer[4] = 2;      // Code für Session-List-Anfrage
    send(sock, buffer, 5, 0);

    // Empfange Session-Liste vom Server (z.B. Größe und Infos)
    char recvBuffer[512];
    int bytesReceived = recv(sock, recvBuffer, 512, 0);
    if (bytesReceived > 0) {
        int offset = 5;
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
    }
}

void ticTacToeGame(SOCKET sock, int sessionNumber) {
    clearScreen();
    std::cout << "You joined session: " << sessionNumber << std::endl;
    std::cout << "Tic Tac Toe game will now start...\n";
    // Hier kommt die Logik für das Tic-Tac-Toe-Spiel
    // Empfange Spielfeld und Zugstatus vom Server und aktualisiere das Spielfeld entsprechend
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
                std::cout << "Enter password: ";
                std::cin >> password;

                int nameLength = playerName.size();
                int passwordLength = password.size();
                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + nameLength + 1 + passwordLength;
                buffer[4] = 1;  // Code für Registrierung
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

                int nameLength = playerName.size();
                int passwordLength = password.size();
                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + nameLength + 1 + passwordLength;
                buffer[4] = 3;  // Code für Login
                buffer[5] = static_cast<char>(nameLength);
                memcpy(buffer + 6, playerName.c_str(), nameLength);
                buffer[6 + nameLength] = static_cast<char>(passwordLength);
                memcpy(buffer + 7 + nameLength, password.c_str(), passwordLength);
                send(sock, buffer, 7 + nameLength + passwordLength, 0);

                // Hier sollte auf die Bestätigung des Servers gewartet werden
                // Wenn erfolgreich, setze loggedIn = true
                loggedIn = true;
            }
            break;

        case 3:  // Request Session List
            if (loggedIn) {
                requestSessionList(sock);
            }
            break;

        case 4:  // Create Session
            if (loggedIn) {
                std::string sessionPassword;
                std::cout << "Enter session password (leave empty if no password): ";
                std::cin >> sessionPassword;

                int passwordLength = sessionPassword.size();
                char buffer[512] = { 0 };
                *(int*)buffer = 1 + passwordLength;
                buffer[4] = 5;  // Code für Session erstellen
                buffer[5] = static_cast<char>(passwordLength);
                if (passwordLength > 0) {
                    memcpy(buffer + 6, sessionPassword.c_str(), passwordLength);
                }
                send(sock, buffer, 6 + passwordLength, 0);
            }
            break;

        case 5:  // Join Session
            if (loggedIn) {
                int sessionNumber;
                std::string sessionPassword;
                std::cout << "Enter session number: ";
                std::cin >> sessionNumber;
                std::cout << "Enter session password (if required): ";
                std::cin >> sessionPassword;

                int passwordLength = sessionPassword.size();
                char buffer[512] = { 0 };
                *(int*)buffer = 9 + passwordLength;
                buffer[4] = 6;  // Code für Session beitreten
                *(int*)(buffer + 5) = sessionNumber;
                buffer[9] = static_cast<char>(passwordLength);
                if (passwordLength > 0) {
                    memcpy(buffer + 10, sessionPassword.c_str(), passwordLength);
                }
                send(sock, buffer, 10 + passwordLength, 0);

                ticTacToeGame(sock, sessionNumber);
            }
            break;

        case 6:  // Logout
            if (loggedIn) {
                char buffer[5] = { 0 };
                *(int*)buffer = 1;
                buffer[4] = 4;  // Code für Logout
                send(sock, buffer, 5, 0);
                loggedIn = false;
                sessionList.clear();
            }
            break;

        case 0:  // Exit
            std::cout << "Exiting...\n";
            break;

        default:
            std::cout << "Invalid option, please try again.\n";
        }
    } while (choice != 0);
}

int main() {
    // Winsock initialisieren
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(54000);

    // Verwenden Sie inet_pton anstelle von inet_addr
    inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr));

    connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));

    handleClient(sock);

    closesocket(sock);
    WSACleanup();
    return 0;
}
