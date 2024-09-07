#define WIN32_LEAN_AND_MEAN
#include <vector>
#include <map>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>   // Für atomare Operationen zur Generierung von Session-IDs

// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

std::mutex sessionMutex;  // Mutex für den Zugriff auf Sessions
std::atomic<int> sessionIdCounter(1); // Atomare Zählung für eindeutige Session-IDs

// In-Memory Benutzerdatenbank (wird im RAM gespeichert und gelöscht, wenn der Server heruntergefahren wird)
std::map<std::string, std::string> users;  // Username -> Passwort

// Tic-Tac-Toe-Spiel-Sitzung
struct Session {
    int sessionId;  // Eindeutige ID der Session
    std::string player1;
    std::string player2;
    bool isFull = false;
    char board[3][3];  // Das Spielfeld
    char currentTurn = 'X';  // Spieler X beginnt

    Session(const std::string& p1) : player1(p1), isFull(false), sessionId(sessionIdCounter++) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; j++) {  // Verwende j++ statt ++j
                board[i][j] = ' ';
            }
        }
    }
};

// Liste der Sitzungen
std::vector<Session> sessions;

// Funktion zur Registrierung eines Benutzers
bool registerUser(const std::string& username, const std::string& password) {
    if (users.find(username) != users.end()) {
        return false;  // Benutzername existiert bereits
    }
    users[username] = password;
    return true;
}

// Funktion zur Anmeldung eines Benutzers
bool loginUser(const std::string& username, const std::string& password) {
    if (users.find(username) != users.end() && users[username] == password) {
        return true;  // Erfolgreich eingeloggt
    }
    return false;  // Falsches Passwort oder Benutzer existiert nicht
}

// Funktion zum Erstellen einer neuen Sitzung
int createSession(const std::string& player1) {
    std::lock_guard<std::mutex> lock(sessionMutex);  // Lock den Mutex
    Session newSession(player1);
    sessions.push_back(newSession);
    std::cout << "Neue Session erstellt: ID=" << newSession.sessionId << ", Spieler1: " << player1 << std::endl;
    return newSession.sessionId;  // Gib die eindeutige ID der neuen Sitzung zurück
}

// Funktion zum Beitreten einer Sitzung
bool joinSession(int sessionId, const std::string& player2) {
    std::lock_guard<std::mutex> lock(sessionMutex);  // Lock den Mutex
    for (Session& session : sessions) {
        if (session.sessionId == sessionId && !session.isFull) {
            session.player2 = player2;
            session.isFull = true;
            std::cout << "Spieler2: " << player2 << " ist der Session ID: " << sessionId << " beigetreten." << std::endl;
            return true;
        }
    }
    std::cout << "Fehler: Session ID " << sessionId << " ist entweder voll oder nicht vorhanden." << std::endl;
    return false;
}

// Funktion zum Löschen einer Sitzung anhand der Session-ID
bool deleteSession(int sessionId, const std::string& player, SOCKET& clientSocket) {
    std::lock_guard<std::mutex> lock(sessionMutex);  // Lock den Mutex
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        if (it->sessionId == sessionId && it->player1 == player) {
            std::cout << "Session ID: " << sessionId << " von Spieler " << player << " wird gelöscht." << std::endl;
            sessions.erase(it);

            // Sende 'SESSION_DELETED' an den Client und überprüfe, ob die Nachricht erfolgreich gesendet wurde
            std::string response = "SESSION_DELETED";
            int sendResult = send(clientSocket, response.c_str(), response.size() + 1, 0);
            if (sendResult == SOCKET_ERROR) {
                std::cout << "Fehler beim Senden von 'SESSION_DELETED': " << WSAGetLastError() << std::endl;
                return false;
            }
            else {
                std::cout << "Nachricht 'SESSION_DELETED' erfolgreich an den Client gesendet." << std::endl;
                return true;
            }
        }
    }
    std::cout << "Fehler beim Löschen der Session: ID " << sessionId << " nicht gefunden oder Spieler " << player << " ist nicht berechtigt." << std::endl;
    return false;
}

// **Funktion zum Auflisten der Sitzungen**
std::string listSessions() {
    std::lock_guard<std::mutex> lock(sessionMutex);  // Lock den Mutex
    std::string sessionList;
    for (const Session& session : sessions) {
        if (!session.isFull) {
            sessionList += "Session ID: " + std::to_string(session.sessionId) + " | Spieler 1: " + session.player1 + "\n";
        }
    }
    if (sessionList.empty()) {
        sessionList = "Keine verfügbaren Sessions.\n";
    }
    return sessionList;
}

// Funktion zur Verarbeitung von Nachrichten vom Client
void processClientMessage(SOCKET& clientSocket, const std::string& message, const std::string& username) {
    std::cout << "Nachricht vom Client: " << message << std::endl;  // Debug-Ausgabe

    if (message.substr(0, 9) == "REGISTER:") {
        size_t firstColon = message.find(':', 9);
        std::string username = message.substr(9, firstColon - 9);
        std::string password = message.substr(firstColon + 1);

        if (registerUser(username, password)) {
            std::string success = "REGISTRATION_SUCCESS";
            send(clientSocket, success.c_str(), success.size() + 1, 0);
        }
        else {
            std::string failure = "REGISTRATION_FAILED";
            send(clientSocket, failure.c_str(), failure.size() + 1, 0);
        }

    }
    else if (message.substr(0, 6) == "LOGIN:") {
        size_t firstColon = message.find(':', 6);
        std::string username = message.substr(6, firstColon - 6);
        std::string password = message.substr(firstColon + 1);

        if (loginUser(username, password)) {
            std::string success = "LOGIN_SUCCESS";
            send(clientSocket, success.c_str(), success.size() + 1, 0);
        }
        else {
            std::string failure = "LOGIN_FAILED";
            send(clientSocket, failure.c_str(), failure.size() + 1, 0);
        }

    }
    else if (message == "CREATE_SESSION") {
        int sessionId = createSession(username);
        std::string response = "SESSION_CREATED:ID=" + std::to_string(sessionId);
        send(clientSocket, response.c_str(), response.size() + 1, 0);

    }
    else if (message.substr(0, 12) == "JOIN_SESSION") {
        std::string sessionIdStr = message.substr(13);
        int sessionId = std::stoi(sessionIdStr);

        if (joinSession(sessionId, username)) {
            std::string response = "JOIN_SUCCESS:Session=" + std::to_string(sessionId);
            send(clientSocket, response.c_str(), response.size() + 1, 0);
        }
        else {
            std::string response = "JOIN_FAILED";
            send(clientSocket, response.c_str(), response.size() + 1, 0);
        }

    }
    else if (message == "LIST_SESSIONS") {
        std::string sessionList = listSessions();
        send(clientSocket, sessionList.c_str(), sessionList.size() + 1, 0);

    }
    else if (message.substr(0, 13) == "DELETE_SESSION") {
        std::string sessionIdStr = message.substr(14);
        int sessionId = std::stoi(sessionIdStr);

        // Löschung der Session
        if (deleteSession(sessionId, username, clientSocket)) {  // Socket wird korrekt übergeben
            std::cout << "Nachricht 'SESSION_DELETED' gesendet." << std::endl;  // Debugging
        }
        else {
            std::string response = "DELETE_FAILED";
            send(clientSocket, response.c_str(), response.size() + 1, 0);
            std::cout << "Fehler beim Senden der Nachricht 'DELETE_FAILED'." << std::endl;
        }
    }
}

// Funktion für jeden Client-Thread
void handleClient(SOCKET clientSocket, const std::string& username) {
    char buffer[512];
    ZeroMemory(buffer, 512);

    while (true) {
        int iResult = recv(clientSocket, buffer, 512, 0);
        if (iResult > 0) {
            std::cout << "Nachricht vom Client (" << username << "): " << buffer << std::endl;
            processClientMessage(clientSocket, buffer, username);  // Nachricht verarbeiten
        }
        else {
            std::cout << "Verbindung zu Client " << username << " verloren." << std::endl;
            closesocket(clientSocket);
            return;
        }
    }
}

int main() {
    WSADATA wsaData;
    int iResult;

    // Initialisiere Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup fehlgeschlagen: " << iResult << std::endl;
        return 1;
    }

    // Erstelle einen Server-Socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cout << "Fehler beim Erstellen des Sockets: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Konfiguriere die Server-Adresse
    sockaddr_in serverHint;
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(54000);  // Port 54000
    serverHint.sin_addr.s_addr = INADDR_ANY;  // Akzeptiere Verbindungen von jeder IP-Adresse

    // Binde den Socket an die IP-Adresse und den Port
    iResult = bind(listenSocket, (sockaddr*)&serverHint, sizeof(serverHint));
    if (iResult == SOCKET_ERROR) {
        std::cout << "Bind fehlgeschlagen: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Lausche auf Verbindungen
    iResult = listen(listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        std::cout << "Listen fehlgeschlagen: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server läuft und wartet auf Verbindungen..." << std::endl;

    // Unbegrenzt auf neue Clients warten
    while (true) {
        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cout << "Fehler bei der Annahme der Verbindung: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }

        std::cout << "Client verbunden!" << std::endl;

        // Benutzername muss hier festgelegt werden (einfache Annahme für Demonstration)
        std::string username = "User" + std::to_string(rand() % 1000);

        // Starte einen neuen Thread für den Client
        std::thread clientThread(handleClient, clientSocket, username);
        clientThread.detach();  // Führe den Thread unabhängig aus
    }

    closesocket(listenSocket);
    WSACleanup();

    return 0;
}
