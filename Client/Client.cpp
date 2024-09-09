#define _WIN32_WINNT 0x0600  // Windows Vista oder höher
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#pragma comment(lib, "ws2_32.lib")  // Link zu Winsock-Bibliotheken

bool loggedIn = false;  // Status, ob der Nutzer eingeloggt ist
std::string playerName;  // Spielername
std::vector<std::string> sessionList;  // Liste der verfügbaren Sitzungen

// Funktion zum Löschen des Bildschirms (für Windows)
void clearScreen() {
    system("cls");
}

// Funktion zum Drucken des Hauptmenüs
void printMenu() {
    clearScreen();
    std::cout << "Tic Tac Toe Game Menu\n";

    if (!loggedIn) {  // Zeige das Registrierungs-/Login-Menü, wenn der Benutzer nicht eingeloggt ist
        std::cout << "1. Register\n";
        std::cout << "2. Login\n";
    }
    else {  // Zeige das Spielmenü, wenn der Benutzer eingeloggt ist
        std::cout << "3. Request Session List\n";
        std::cout << "4. Create Session\n";
        std::cout << "5. Join Session\n";
        std::cout << "6. Logout\n";

        if (!sessionList.empty()) {  // Zeige die Liste der verfügbaren Sitzungen, wenn vorhanden
            std::cout << "\nAvailable Sessions:\n";
            for (const auto& session : sessionList) {
                std::cout << session << std::endl;
            }
        }
    }

    std::cout << "0. Exit\n";
}

// Funktion zum Anfordern der Sitzungsliste vom Server
void requestSessionList(SOCKET sock) {
    // Nachricht an den Server senden, um die Sitzungsliste anzufordern
    char buffer[5] = { 0 };
    *(int*)buffer = 1;  // Nachrichtengröße
    buffer[4] = 2;      // Code für Anforderung der Sitzungsliste
    send(sock, buffer, 5, 0);

    // Antwort vom Server empfangen
    char recvBuffer[512];
    int bytesReceived = recv(sock, recvBuffer, 512, 0);
    if (bytesReceived > 0) {
        int offset = 5;
        sessionList.clear();  // Sitzungsliste leeren
        while (offset < bytesReceived) {
            int sessionNumber = *(int*)(recvBuffer + offset);  // Sitzungsnummer auslesen
            offset += 4;
            int enemyNameLength = recvBuffer[offset++];  // Länge des Gegnernamens
            bool passwordProtected = recvBuffer[offset++];  // Ob die Sitzung passwortgeschützt ist

            // Informationen über die Sitzung zusammenstellen
            std::string sessionInfo = "Session " + std::to_string(sessionNumber);
            sessionInfo += enemyNameLength == 0 ? " (Waiting for player)" : " (Enemy)";
            sessionInfo += passwordProtected ? " [Password Protected]" : " [Open]";
            sessionList.push_back(sessionInfo);  // Sitzung zur Liste hinzufügen
        }
    }
}

// Funktion zum Starten des Tic-Tac-Toe-Spiels (noch ohne Logik implementiert)
void ticTacToeGame(SOCKET sock, int sessionNumber) {
    clearScreen();
    std::cout << "You joined session: " << sessionNumber << std::endl;
    std::cout << "Tic Tac Toe game will now start...\n";
    // Hier könnte die Spiellogik implementiert werden
}

// Hauptfunktion zur Verwaltung des Clients
void handleClient(SOCKET sock) {
    int choice;

    do {
        printMenu();  // Zeige das Menü
        std::cout << "Enter your choice: ";  // Benutzerauswahl eingeben
        std::cin >> choice;

        switch (choice) {
        case 1:  // Registrierung
            if (!loggedIn) {
                std::cout << "Enter name: ";
                std::cin >> playerName;
                std::string password;
                std::cout << "Enter password (max 255 chars, no spaces): ";
                std::cin >> password;

                // Überprüfung der Eingaben auf ungültige Zeichen oder leere Felder
                if (password.empty() || password.find(' ') != std::string::npos || playerName.empty()) {
                    std::cout << "Invalid input! Password and name cannot be empty or contain spaces.\n";
                    break;
                }

                // Nachricht zur Registrierung vorbereiten
                int nameLength = playerName.size();
                int passwordLength = password.size();
                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + nameLength + 1 + passwordLength;  // Nachrichtengröße
                buffer[4] = 1;  // Nachrichtencode für Registrierung
                buffer[5] = static_cast<char>(nameLength);  // Länge des Spielernamens
                memcpy(buffer + 6, playerName.c_str(), nameLength);  // Spielernamen hinzufügen
                buffer[6 + nameLength] = static_cast<char>(passwordLength);  // Länge des Passworts
                memcpy(buffer + 7 + nameLength, password.c_str(), passwordLength);  // Passwort hinzufügen
                send(sock, buffer, 7 + nameLength + passwordLength, 0);  // Nachricht an den Server senden

                // Antwort vom Server empfangen
                char recvBuffer[5];
                int bytesReceived = recv(sock, recvBuffer, 5, 0);
                if (bytesReceived > 0 && recvBuffer[4] == 101) {
                    std::cout << "Registration successful! Logging you in...\n";
                    loggedIn = true;  // Erfolgreiche Registrierung, Benutzer wird eingeloggt
                }
                else if (bytesReceived > 0 && recvBuffer[4] == 102) {
                    std::cout << "Error: Name already taken!\n";  // Fehlermeldung bei doppeltem Namen
                }
            }
            break;

        case 2:  // Login
            if (!loggedIn) {
                std::cout << "Enter name: ";
                std::cin >> playerName;
                std::string password;
                std::cout << "Enter password: ";
                std::cin >> password;

                // Überprüfung auf leere Eingaben
                if (playerName.empty() || password.empty()) {
                    std::cout << "Name and password cannot be empty.\n";
                    break;
                }

                // Nachricht zum Login vorbereiten
                int nameLength = playerName.size();
                int passwordLength = password.size();
                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + nameLength + 1 + passwordLength;
                buffer[4] = 3;  // Nachrichtencode für Login
                buffer[5] = static_cast<char>(nameLength);
                memcpy(buffer + 6, playerName.c_str(), nameLength);
                buffer[6 + nameLength] = static_cast<char>(passwordLength);
                memcpy(buffer + 7 + nameLength, password.c_str(), passwordLength);
                send(sock, buffer, 7 + nameLength + passwordLength, 0);

                // Antwort vom Server empfangen
                char recvBuffer[5];
                int bytesReceived = recv(sock, recvBuffer, 5, 0);
                if (bytesReceived > 0 && recvBuffer[4] == 104) {
                    std::cout << "Login successful!\n";
                    loggedIn = true;
                }
                else if (bytesReceived > 0 && recvBuffer[4] == 105) {
                    std::cout << "Error: Incorrect name or password.\n";
                }
            }
            break;

        case 3:  // Sitzungsliste anfordern
            if (loggedIn) {
                requestSessionList(sock);
            }
            break;

        case 4:  // Create session
            if (loggedIn) {
                std::string sessionPassword;
                std::cout << "Enter session password (leave empty if no password, max 255 chars, no spaces): ";
                std::cin >> sessionPassword;

                // Check for invalid input
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
                buffer[4] = 5;  // Nachrichtencode für Login
                buffer[5] = static_cast<char>(passwordLength);
                memcpy(buffer + 6, sessionPassword.c_str(), passwordLength);
                send(sock, buffer, 6 + passwordLength, 0);


                // Wait for server response
                char recvBuffer[512];
                int bytesReceived = recv(sock, recvBuffer, 512, 0);
                if (bytesReceived > 0) {
                    // Handle server response
                    if (recvBuffer[4] == 108) {  // 108: "Session created successfully"
                        std::cout << "Session created successfully!\n";
                    }
                    else if (recvBuffer[4] == 112) {  // 112: Error code (e.g., session limit reached)
                        std::cout << "Error: Session limit reached or other error.\n";
                    }
                }
                else {
                    std::cout << "Error: No response from the server.\n";
                }
            }
            break;




        case 5:  // Sitzung beitreten
            if (loggedIn) {
                int sessionNumber;
                std::string sessionPassword;
                std::cout << "Enter session number: ";
                std::cin >> sessionNumber;
                std::cout << "Enter session password (if required): ";
                std::cin >> sessionPassword;

                // Überprüfung auf ungültige Eingaben
                if (sessionPassword.find(' ') != std::string::npos) {
                    std::cout << "Invalid input! Password cannot contain spaces.\n";
                    break;
                }

                int passwordLength = sessionPassword.size();

                char buffer[512] = { 0 };
                *(int*)buffer = 1 + 1 + 1 + passwordLength;
                buffer[4] = 6;  // Nachrichtencode für Login
                buffer[5] = sessionNumber;
                buffer[6] = passwordLength;
                memcpy(buffer + 7, sessionPassword.c_str(), passwordLength);
                send(sock, buffer, 7 + passwordLength, 0);

                // Tic-Tac-Toe-Spiel starten
                ticTacToeGame(sock, sessionNumber);
                system("pause");
            }
            break;

        case 6:  // Logout
            if (loggedIn) {
                char buffer[5] = { 0 };
                *(int*)buffer = 1;
                buffer[4] = 4;  // Nachrichtencode für Logout
                send(sock, buffer, 5, 0);

                // Serverantwort (optional)
                char recvBuffer[5];
                int bytesReceived = recv(sock, recvBuffer, 5, 0);
                if (bytesReceived > 0 && recvBuffer[4] == 104) {
                    std::cout << "Logged out successfully!\n";
                }

                loggedIn = false;  // Benutzer wird ausgeloggt
                sessionList.clear();  // Sitzungsliste leeren
                std::cout << "Logged out.\n";
            }
            break;

        case 0:  // Programm beenden
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

    // Client-Socket erstellen
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(3400);

    // IP-Adresse des Servers setzen
    inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr));

    // Verbindung zum Server herstellen
    connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // Client-Logik starten
    handleClient(sock);

    // Socket schließen und Winsock aufräumen
    closesocket(sock);
    WSACleanup();
    return 0;
}
