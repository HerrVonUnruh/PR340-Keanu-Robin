#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdlib>
#include <thread>
#include <chrono>

// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// Funktion zum Clearen der Konsole
void clearConsole() {
    system("cls");  // Clear für Windows, unter Linux/Mac wäre es "clear"
}

// Funktion zum Setzen eines Timeouts für den Empfang auf dem Client
void setRecvTimeout(SOCKET clientSocket, int timeoutSec) {
    // Timeout in Millisekunden umrechnen
    int timeoutMs = timeoutSec * 1000;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));
}

void registerUser(SOCKET& clientSocket, const std::string& username, const std::string& password) {
    std::string message = "REGISTER:" + username + ":" + password;
    send(clientSocket, message.c_str(), message.size() + 1, 0);
    std::cout << "Register-Nachricht gesendet: " << message << std::endl;
}

void loginUser(SOCKET& clientSocket, const std::string& username, const std::string& password) {
    std::string message = "LOGIN:" + username + ":" + password;
    send(clientSocket, message.c_str(), message.size() + 1, 0);
    std::cout << "Login-Nachricht gesendet: " << message << std::endl;
}

std::string currentSession = "";  // Speichert die Session-ID

// Funktion zum Löschen einer Session
void deleteSession(SOCKET& clientSocket) {
    if (!currentSession.empty()) {
        std::string message = "DELETE_SESSION:" + currentSession;
        send(clientSocket, message.c_str(), message.size() + 1, 0);
        std::cout << "DELETE_SESSION-Nachricht gesendet: " << message << std::endl;

        // Antwort vom Server abwarten
        char buffer[512];
        ZeroMemory(buffer, 512);

        // Timeout für das Empfangsverhalten setzen (10 Sekunden)
        setRecvTimeout(clientSocket, 10);

        int iResult = recv(clientSocket, buffer, 512, 0);
        if (iResult > 0) {
            std::string response(buffer);
            std::cout << "Antwort vom Server empfangen: " << response << std::endl;

            if (response == "SESSION_DELETED") {
                std::cout << "Session erfolgreich gelöscht.\n";
                currentSession.clear();  // Session-ID zurücksetzen
            }
            else {
                std::cout << "Fehler: Unerwartete Antwort vom Server: " << response << std::endl;
            }
        }
        else if (iResult == 0) {
            std::cout << "Verbindung wurde geschlossen." << std::endl;
        }
        else {
            std::cout << "Fehler beim Empfang der Serverantwort: " << WSAGetLastError() << std::endl;
        }
    }
}

// Funktion, um das Menü nach erfolgreichem Login/Registrierung anzuzeigen
void showMenu(SOCKET& clientSocket) {
    std::string input;
    bool running = true;

    while (running) {
        clearConsole();  // Clear die Konsole vor jedem Menü

        // Überprüfe, ob der Benutzer eine Session hat
        if (currentSession.empty()) {
            std::cout << "\nMenü:\n";
            std::cout << "1. Erstelle eine Session\n";
            std::cout << "2. Tritt einer Session bei\n";
            std::cout << "3. Spiel beenden\n";
        }
        else {
            std::cout << "\nMenü:\n";
            std::cout << "1. Session ID: " << currentSession << "\n";
            std::cout << "2. Lösche deine aktuelle Session\n";
            std::cout << "3. Spiel beenden\n";
        }

        std::cout << "Deine Wahl: ";
        std::cin >> input;

        if (input == "1" && currentSession.empty()) {
            std::string message = "CREATE_SESSION";
            send(clientSocket, message.c_str(), message.size() + 1, 0);

            // Antwort vom Server empfangen
            char buffer[512];
            ZeroMemory(buffer, 512);
            int iResult = recv(clientSocket, buffer, 512, 0);
            if (iResult > 0) {
                std::string response(buffer);
                std::cout << "Antwort vom Server empfangen: " << response << std::endl;

                // Session-ID korrekt extrahieren (Nur die Zahl extrahieren, ohne ":ID=")
                size_t idStart = response.find("ID=") + 3;
                currentSession = response.substr(idStart);  // Extrahiere nur die Session-ID
            }
            else {
                std::cout << "Fehler beim Empfang der Serverantwort: " << WSAGetLastError() << std::endl;
            }
        }
        else if (input == "2" && currentSession.empty()) {
            std::string message = "LIST_SESSIONS";
            send(clientSocket, message.c_str(), message.size() + 1, 0);

            // Antwort vom Server empfangen
            char buffer[512];
            ZeroMemory(buffer, 512);
            int iResult = recv(clientSocket, buffer, 512, 0);
            if (iResult > 0) {
                std::string response(buffer);
                std::cout << "Antwort vom Server empfangen: " << response << std::endl;

                std::string sessionId;
                bool validSessionId = false;

                // Schleife zur Validierung der Session-ID
                while (!validSessionId) {
                    std::cout << "\nMöchtest du einer Session beitreten? (j/n): ";
                    std::cin >> input;

                    if (input == "j") {
                        std::cout << "Gib die Session-ID ein, der du beitreten möchtest: ";
                        std::cin >> sessionId;

                        // Versuche die Session-ID in eine Zahl zu konvertieren
                        try {
                            int sessionIdNum = std::stoi(sessionId);  // Konvertiere in eine Zahl

                            // Wenn die Konvertierung erfolgreich war, sende die Beitrittsnachricht
                            std::string message = "JOIN_SESSION:" + sessionId;
                            send(clientSocket, message.c_str(), message.size() + 1, 0);
                            validSessionId = true;  // Setze validSessionId auf true, um die Schleife zu beenden

                        }
                        catch (const std::invalid_argument&) {
                            // Fehlermeldung bei nicht numerischer Eingabe
                            std::cout << "Ungültige Session-ID. Bitte gib eine Zahl ein.\n";
                        }
                        catch (const std::out_of_range&) {
                            // Fehlermeldung bei zu großer Zahl
                            std::cout << "Session-ID ist zu groß. Bitte gib eine gültige Zahl ein.\n";
                        }
                    }
                    else if (input == "n") {
                        break;  // Wenn der Benutzer "n" eingibt, beende die Schleife
                    }
                    else {
                        std::cout << "Ungültige Auswahl, bitte wähle erneut.\n";
                    }
                }
            }
            else {
                std::cout << "Fehler beim Empfang der Serverantwort: " << WSAGetLastError() << std::endl;
            }
        }
        else if (input == "2" && !currentSession.empty()) {
            deleteSession(clientSocket);  // Session löschen und dann ins Menü zurückkehren
        }
        else if (input == "3") {
            // Überprüfe, ob der Benutzer eine Session hat und lösche diese, falls vorhanden
            deleteSession(clientSocket);

            std::cout << "Spiel wird beendet..." << std::endl;
            running = false;
        }
        else {
            std::cout << "Ungültige Auswahl, bitte wähle erneut." << std::endl;
        }
    }

    // Schließe den Socket und beende sauber
    std::cout << "Verbindung wird geschlossen...\n";
    closesocket(clientSocket);
    WSACleanup();
    std::cout << "Spiel beendet.\n";
}

int main() {
    WSADATA wsaData;
    int iResult;

    // Initialisiere Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    // Erstelle einen Client-Socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Server-Adresse konfigurieren
    sockaddr_in serverHint;
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(54000);  // Gleicher Port wie der Server
    inet_pton(AF_INET, "127.0.0.1", &serverHint.sin_addr);  // Verbinde zu localhost (127.0.0.1)

    // Verbindung zum Server herstellen
    iResult = connect(clientSocket, (sockaddr*)&serverHint, sizeof(serverHint));
    if (iResult == SOCKET_ERROR) {
        std::cout << "Connect failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Verbunden mit dem Server!" << std::endl;

    // Benutzer nach Registrierung oder Login fragen
    std::string action;
    bool loginFailed = false;  // Variable, um fehlerhaften Login zu verfolgen
    bool invalidInput = false; // Variable, um fehlerhafte Eingabe zu verfolgen

    // Eingabevalidierungsschleife für Registrierung oder Login
    do {
        clearConsole();  // Clear die Konsole vor jeder Eingabeaufforderung

        if (invalidInput) {  // Wenn eine ungültige Eingabe gemacht wurde, wird die Fehlermeldung angezeigt
            std::cout << "Falsche Eingabe, versuche es erneut.\n";
        }

        std::cout << "Möchtest du dich registrieren oder einloggen? (registrieren/einloggen): ";
        std::cin >> action;

        invalidInput = (action != "registrieren" && action != "einloggen");  // Setze invalidInput auf true bei falscher Eingabe

    } while (invalidInput);  // Wiederhole bis richtige Eingabe erfolgt

    // Benutzernamen und Passwort abfragen
    std::string username, password;
    std::cout << "Benutzername: ";
    std::cin >> username;
    std::cout << "Passwort: ";
    std::cin >> password;

    // Nachricht je nach Aktion senden
    if (action == "registrieren") {
        registerUser(clientSocket, username, password);
    }
    else if (action == "einloggen") {
        loginUser(clientSocket, username, password);
    }

    // Antwort vom Server empfangen
    char buffer[512];
    ZeroMemory(buffer, 512);
    iResult = recv(clientSocket, buffer, 512, 0);

    // Schleife für Registrierungs-Fehlerbehandlung (Falls Benutzername bereits vergeben)
    while (iResult > 0 && std::string(buffer) == "REGISTRATION_FAILED") {
        clearConsole();  // Clear die Konsole bei falscher Registrierung
        std::cout << "Benutzername existiert bereits, bitte versuche es erneut.\n";

        // Eingabeaufforderung erneut anzeigen
        do {
            std::cout << "Möchtest du dich registrieren oder einloggen? (registrieren/einloggen): ";
            std::cin >> action;

        } while (action != "registrieren" && action != "einloggen");

        std::cout << "Benutzername: ";
        std::cin >> username;
        std::cout << "Passwort: ";
        std::cin >> password;

        if (action == "registrieren") {
            registerUser(clientSocket, username, password);
        }
        else if (action == "einloggen") {
            loginUser(clientSocket, username, password);
        }

        ZeroMemory(buffer, 512);
        iResult = recv(clientSocket, buffer, 512, 0);
    }

    // Schleife für Login-Fehlerbehandlung
    while (iResult > 0 && std::string(buffer) == "LOGIN_FAILED") {
        loginFailed = true;  // Setze Login-Fehler auf true
        clearConsole();  // Clear die Konsole bei falschem Login
        std::cout << "Falscher Username oder Passwort, bitte versuche es erneut.\n";

        // Eingabeaufforderung erneut anzeigen
        do {
            std::cout << "Möchtest du dich registrieren oder einloggen? (registrieren/einloggen): ";
            std::cin >> action;

        } while (action != "registrieren" && action != "einloggen");

        std::cout << "Benutzername: ";
        std::cin >> username;
        std::cout << "Passwort: ";
        std::cin >> password;

        if (action == "registrieren") {
            registerUser(clientSocket, username, password);
        }
        else if (action == "einloggen") {
            loginUser(clientSocket, username, password);
        }

        ZeroMemory(buffer, 512);
        iResult = recv(clientSocket, buffer, 512, 0);
    }

    if (iResult > 0 && (std::string(buffer) == "LOGIN_SUCCESS" || std::string(buffer) == "REGISTRATION_SUCCESS")) {
        std::cout << "Server Antwort: " << buffer << std::endl;
        showMenu(clientSocket);  // Zeige das Menü nach erfolgreichem Login/Registrierung
    }
    else {
        std::cout << "Fehler beim Empfang der Serverantwort: " << WSAGetLastError() << std::endl;
    }

    return 0;
}
