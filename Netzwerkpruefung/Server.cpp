#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>  // Für std::mutex und std::lock_guard
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

struct Session {
    int sessionId;
    std::string player1;
    std::string player2;
    bool isFull = false;

    Session(int id, const std::string& p1) : sessionId(id), player1(p1), isFull(false) {}
};

std::map<std::string, std::string> users;  // In-Memory Benutzerdatenbank: username -> password
std::vector<Session> sessions;
int sessionCounter = 1;
std::mutex sessionMutex;  // Schutz vor gleichzeitigen Zugriffen auf Sitzungsdaten

// Funktion zum Senden von Nachrichten an den Client
void sendToClient(SOCKET clientSocket, const std::string& message) {
    send(clientSocket, message.c_str(), message.size() + 1, 0);
}

// Benutzerregistrierung
bool registerUser(const std::string& username, const std::string& password) {
    if (users.find(username) != users.end()) {
        return false;  // Benutzername existiert bereits
    }
    users[username] = password;  // Speichere Benutzername und Passwort
    return true;
}

// Benutzeranmeldung
bool loginUser(const std::string& username, const std::string& password) {
    if (users.find(username) != users.end() && users[username] == password) {
        return true;  // Login erfolgreich
    }
    return false;  // Benutzername existiert nicht oder Passwort ist falsch
}

// Überprüfen, ob der Benutzer bereits eine aktive Session hat
bool userHasSession(const std::string& username) {
    for (const auto& session : sessions) {
        if (session.player1 == username) {
            return true;  // Benutzer hat bereits eine Session
        }
    }
    return false;
}

// Session erstellen
void createSession(SOCKET clientSocket, const std::string& player1) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    if (userHasSession(player1)) {
        sendToClient(clientSocket, "SESSION_FAILED: Du hast bereits eine Session erstellt.");
        return;
    }

    Session newSession(sessionCounter++, player1);
    sessions.push_back(newSession);
    sendToClient(clientSocket, "SESSION_CREATED: ID=" + std::to_string(newSession.sessionId));
}

// Liste der offenen Sessions senden
void listSessions(SOCKET clientSocket) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    std::string sessionList;
    for (const auto& session : sessions) {
        if (!session.isFull) {
            sessionList += "Session ID: " + std::to_string(session.sessionId) + " | Spieler 1: " + session.player1 + "\n";
        }
    }
    if (sessionList.empty()) {
        sessionList = "Keine verfügbaren Sessions.\n";
    }
    sendToClient(clientSocket, sessionList);
}

// Session beitreten
void joinSession(SOCKET clientSocket, int sessionId, const std::string& player2) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    for (auto& session : sessions) {
        if (session.sessionId == sessionId && !session.isFull) {
            session.player2 = player2;
            session.isFull = true;
            sendToClient(clientSocket, "SESSION_JOINED: SessionID=" + std::to_string(sessionId));
            return;
        }
    }
    sendToClient(clientSocket, "JOIN_FAILED: Session ist voll oder nicht vorhanden.");
}

// Verarbeitung von Nachrichten vom Client
void processClientMessage(SOCKET clientSocket, const std::string& message, const std::string& username) {
    if (message.substr(0, 9) == "REGISTER:") {
        size_t firstColon = message.find(':', 9);
        std::string regUsername = message.substr(9, firstColon - 9);
        std::string regPassword = message.substr(firstColon + 1);

        if (registerUser(regUsername, regPassword)) {
            sendToClient(clientSocket, "REGISTRATION_SUCCESS");
        }
        else {
            sendToClient(clientSocket, "REGISTRATION_FAILED: Benutzername bereits vergeben.");
        }
    }
    else if (message.substr(0, 6) == "LOGIN:") {
        size_t firstColon = message.find(':', 6);
        std::string logUsername = message.substr(6, firstColon - 6);
        std::string logPassword = message.substr(firstColon + 1);

        if (loginUser(logUsername, logPassword)) {
            sendToClient(clientSocket, "LOGIN_SUCCESS");
        }
        else {
            sendToClient(clientSocket, "LOGIN_FAILED: Falscher Benutzername oder Passwort.");
        }
    }
    else if (message == "CREATE_SESSION") {
        createSession(clientSocket, username);
    }
    else if (message == "LIST_SESSIONS") {
        listSessions(clientSocket);
    }
    else if (message.substr(0, 11) == "JOIN_SESSION") {
        int sessionId = std::stoi(message.substr(12));
        joinSession(clientSocket, sessionId, username);
    }
    else {
        sendToClient(clientSocket, "UNKNOWN_COMMAND");
    }
}

// Funktion für jeden Client-Thread
void clientHandler(SOCKET clientSocket) {
    char buffer[1024];
    std::string username = "Spieler";  // Placeholder für echten Benutzernamen

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, 1024, 0);
        if (bytesReceived <= 0) {
            break;  // Verbindung wurde unterbrochen
        }
        buffer[bytesReceived] = '\0';
        std::string message(buffer);
        processClientMessage(clientSocket, message, username);
    }

    closesocket(clientSocket);
}

// Hauptfunktion für den Server
int main() {
    WSADATA wsaData;
    SOCKET listeningSocket, clientSocket;
    sockaddr_in serverAddr, clientAddr;
    int addrLen = sizeof(clientAddr);

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Erstelle einen Server-Socket und binde ihn an die IP-Adresse und Port
    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;
    serverAddr.sin_port = htons(54000);

    bind(listeningSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listeningSocket, SOMAXCONN);

    std::cout << "Server gestartet. Warte auf Verbindungen..." << std::endl;

    // Akzeptiere Clients und erstelle einen Thread für jeden Client
    while (true) {
        clientSocket = accept(listeningSocket, (sockaddr*)&clientAddr, &addrLen);
        std::thread clientThread(clientHandler, clientSocket);
        clientThread.detach();  // Führe den Thread unabhängig aus
    }

    closesocket(listeningSocket);
    WSACleanup();
    return 0;
}
