#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// Funktion zum Clearen der Konsole (nur für Windows)
void clearConsole() {
    system("cls");  // Windows-Befehl zum Leeren der Konsole (bei Linux/Mac wäre es "clear")
}

// Funktion zum Senden von Nachrichten an den Server
void sendToServer(SOCKET serverSocket, const std::string& message) {
    send(serverSocket, message.c_str(), message.size() + 1, 0);
}

// Funktion zum Empfangen von Nachrichten vom Server
std::string receiveFromServer(SOCKET serverSocket) {
    char buffer[1024];  // Puffer für eingehende Daten
    int bytesReceived = recv(serverSocket, buffer, 1024, 0);
    if (bytesReceived <= 0) {
        return "";  // Rückgabe eines leeren Strings bei Verbindungsfehlern oder geschlossener Verbindung
    }
    buffer[bytesReceived] = '\0';  // String-Ende setzen
    return std::string(buffer);
}

// Funktion, um das Hauptmenü anzuzeigen und Aktionen zu ermöglichen (Session erstellen, beitreten, etc.)
void showMenu(SOCKET serverSocket) {
    std::string input;
    bool running = true;
    std::string errorMessage = "";  // Variable zur Speicherung von Fehlermeldungen

    while (running) {
        clearConsole();  // Clear die Konsole vor jedem neuen Menüzyklus

        // Falls eine Fehlermeldung vorliegt, wird sie angezeigt
        if (!errorMessage.empty()) {
            std::cout << "Fehler: " << errorMessage << "\n";
        }

        // Anzeige des Hauptmenüs
        std::cout << "\nMenü:\n";
        std::cout << "1. Session erstellen\n";
        std::cout << "2. Einer Session beitreten\n";
        std::cout << "3. Liste der Sessions anzeigen\n";
        std::cout << "4. Beenden\n";
        std::cout << "Deine Wahl: ";
        std::cin >> input;

        if (input == "1") {  // Session erstellen
            sendToServer(serverSocket, "CREATE_SESSION");
            std::string serverResponse = receiveFromServer(serverSocket);
            std::cout << "Antwort vom Server: " << serverResponse << std::endl;
            errorMessage = "";  // Fehlermeldung zurücksetzen
        }
        else if (input == "2") {  // Session beitreten
            std::cout << "Gib die Session-ID ein: ";
            int sessionId;
            std::cin >> sessionId;
            sendToServer(serverSocket, "JOIN_SESSION:" + std::to_string(sessionId));
            std::string serverResponse = receiveFromServer(serverSocket);
            std::cout << "Antwort vom Server: " << serverResponse << std::endl;
            errorMessage = "";  // Fehlermeldung zurücksetzen
        }
        else if (input == "3") {  // Liste der Sessions anzeigen
            sendToServer(serverSocket, "LIST_SESSIONS");
            std::string serverResponse = receiveFromServer(serverSocket);
            std::cout << "Sessions:\n" << serverResponse << std::endl;
            errorMessage = "";  // Fehlermeldung zurücksetzen
        }
        else if (input == "4") {  // Spiel beenden
            std::cout << "Spiel wird beendet." << std::endl;
            running = false;  // Beende das Programm ohne die Aufforderung zum Drücken von Enter
        }
        else {
            errorMessage = "Ungültige Auswahl, bitte wähle erneut.";  // Setze die Fehlermeldung
        }

        // Nach erfolgreicher Eingabe wird die Konsole nicht gecleart und auf Enter gewartet
        if (running && errorMessage.empty()) {
            std::cout << "\nDrücke Enter, um zum Menü zurückzukehren...";
            std::cin.ignore();  // Leert den Eingabepuffer
            std::cin.get();     // Wartet auf Enter
        }
    }
}

// Funktion zur Handhabung von Login oder Registrierung
void loginOrRegister(SOCKET serverSocket) {
    std::string action;
    bool invalidInput = false;  // Variable, um falsche Eingaben zu erkennen

    // Schleife, die den Benutzer nach Login oder Registrierung fragt
    do {
        clearConsole();  // Clear die Konsole vor jeder neuen Eingabeaufforderung

        // Falls eine ungültige Eingabe gemacht wurde, wird eine Fehlermeldung angezeigt
        if (invalidInput) {
            std::cout << "Falsche Eingabe, versuche es erneut.\n";
        }

        std::cout << "Möchtest du dich registrieren oder einloggen? (registrieren/einloggen): ";
        std::cin >> action;

        // Überprüft, ob die Eingabe gültig ist
        invalidInput = (action != "registrieren" && action != "einloggen");

    } while (invalidInput);  // Schleife läuft, bis eine gültige Eingabe gemacht wird

    std::string username, password;
    std::cout << "Benutzername: ";
    std::cin >> username;
    std::cout << "Passwort: ";
    std::cin >> password;

    // Nachricht zum Registrieren oder Einloggen erstellen
    std::string message;
    if (action == "registrieren") {
        message = "REGISTER:" + username + ":" + password;
    }
    else {
        message = "LOGIN:" + username + ":" + password;
    }

    sendToServer(serverSocket, message);
    std::string response = receiveFromServer(serverSocket);

    // Erfolgreiche Registrierung oder Login
    if (response == "REGISTRATION_SUCCESS" || response == "LOGIN_SUCCESS") {
        std::cout << "Erfolgreich " << (action == "registrieren" ? "registriert" : "eingeloggt") << "!" << std::endl;
        showMenu(serverSocket);  // Menü nach erfolgreicher Anmeldung/Registrierung anzeigen
    }
    else if (response == "REGISTRATION_FAILED: Benutzername bereits vergeben.") {  // Fehler bei der Registrierung
        std::cout << "Fehler: " << response << std::endl;
        loginOrRegister(serverSocket);  // Wiederhole den Registrierungsversuch
    }
    else if (response == "LOGIN_FAILED: Falscher Benutzername oder Passwort.") {  // Fehler beim Login
        std::cout << "Fehler beim Einloggen: " << response << std::endl;  // Zeigt den Grund des Fehlers
        loginOrRegister(serverSocket);  // Wiederhole den Loginversuch
    }
    else {  // Andere Fehlermeldungen
        std::cout << response << std::endl;
        loginOrRegister(serverSocket);  // Wiederhole Login oder Registrierung
    }
}

int main() {
    WSADATA wsaData;
    SOCKET serverSocket;
    sockaddr_in serverAddr;

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Erstellen eines Sockets und Verbindungsaufbau zum Server
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr));  // Nutze inet_pton für IP-Adressen
    serverAddr.sin_port = htons(54000);

    connect(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    std::cout << "Verbunden mit dem Server." << std::endl;

    loginOrRegister(serverSocket);  // Starte die Login-/Registrierungsroutine

    // Schließe den Socket und beende das Programm
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
