#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>

#pragma comment(lib, "Ws2_32.lib")  // Link zur Winsock-Bibliothek

const int PORT = 3400;  // Portnummer für den Server
const int BUFFER_SIZE = 512;  // Größe des Nachrichtenpuffers
const int SESSION_LIMIT = 8;  // Maximale Anzahl von Sitzungen

// Struktur für eine Tic-Tac-Toe-Sitzung
struct Session {
    int sessionNumber;  // Sitzungsnummer
    std::string player1;  // Name des ersten Spielers
    std::string player2;  // Name des zweiten Spielers
    bool passwordProtected;  // Ist die Sitzung passwortgeschützt?
    std::string password;  // Passwort der Sitzung
    char board[9] = { 0 };  // Spielfeld für Tic-Tac-Toe
    bool isPlayer1Turn = true;  // Ist Spieler 1 an der Reihe?
};

// Struktur für einen Benutzer
struct User {
    std::string name;  // Benutzername
    std::string password;  // Benutzerpasswort
    SOCKET socket;  // Socket des Benutzers
    bool loggedIn = false;  // Ist der Benutzer eingeloggt?
    int currentSession = -1;  // Aktuelle Sitzung des Benutzers
};

// Karten zur Speicherung von Benutzern und Sitzungen
std::unordered_map<std::string, User> registeredUsers;
std::unordered_map<int, Session> activeSessions;
std::unordered_map<SOCKET, std::vector<char>> clientBuffers;
int sessionCounter = 0;  // Zähler für die Sitzungsnummern

// Funktion zur Initialisierung von Winsock
void initWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        exit(1);
    }
}

// Funktion zur Erstellung des Listener-Sockets (für eingehende Verbindungen)
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

    // Setzt den Listener-Socket in den nicht-blockierenden Modus
    u_long mode = 1;
    ioctlsocket(listener, FIONBIO, &mode);

    std::cout << "Server started and listening on port " << PORT << std::endl;
    return listener;
}

// Funktion zum Senden von Nachrichten an den Client
void sendMessage(SOCKET clientSocket, char messageCode, const std::vector<char>& data = {}) {
    std::vector<char> message(5 + data.size());
    *(int*)message.data() = (int)(1 + data.size());  // Nachrichtengröße
    message[4] = messageCode;  // Nachrichtencode
    std::copy(data.begin(), data.end(), message.begin() + 5);  // Spezielle Daten hinzufügen
    send(clientSocket, message.data(), message.size(), 0);  // Nachricht senden
}

// Funktion zur Registrierung eines Benutzers
void handleRegister(SOCKET clientSocket, const std::vector<char>& message) {
    int nameLength = message[5];  // Länge des Benutzernamens
    std::string name(message.begin() + 6, message.begin() + 6 + nameLength);  // Benutzername
    int passwordLength = message[6 + nameLength];  // Länge des Passworts
    std::string password(message.begin() + 7 + nameLength, message.begin() + 7 + nameLength + passwordLength);  // Passwort

    if (registeredUsers.find(name) == registeredUsers.end()) {  // Wenn der Benutzername noch nicht existiert
        registeredUsers[name] = { name, password, clientSocket, true, -1 };  // Benutzer hinzufügen und als eingeloggt markieren
        sendMessage(clientSocket, 101);  // Erfolgreiche Registrierung senden
        std::cout << "[SUCCESS] User " << name << " registered and logged in successfully." << std::endl;
    }
    else {
        sendMessage(clientSocket, 102);  // Fehlgeschlagene Registrierung senden (Benutzername bereits vergeben)
        std::cout << "[ERROR] Registration failed, user " << name << " already exists." << std::endl;
    }
}

// Funktion zum Einloggen eines Benutzers
void handleLogin(SOCKET clientSocket, const std::vector<char>& message) {
    int nameLength = message[5];
    std::string name(message.begin() + 6, message.begin() + 6 + nameLength);
    int passwordLength = message[6 + nameLength];
    std::string password(message.begin() + 7 + nameLength, message.begin() + 7 + nameLength + passwordLength);

    auto it = registeredUsers.find(name);  // Benutzer im Speicher suchen
    if (it != registeredUsers.end() && it->second.password == password) {  // Passwort überprüfen
        if (it->second.loggedIn) {  // Falls Benutzer bereits eingeloggt ist
            sendMessage(clientSocket, 105);  // Fehlermeldung senden
            std::cout << "[ERROR] User " << name << " is already logged in." << std::endl;
        }
        else {
            it->second.socket = clientSocket;  // Benutzer-Socket aktualisieren
            it->second.loggedIn = true;  // Als eingeloggt markieren
            sendMessage(clientSocket, 104);  // Erfolgreichen Login senden
            std::cout << "[SUCCESS] User " << name << " logged in successfully." << std::endl;
        }
    }
    else {
        sendMessage(clientSocket, 105);  // Fehlgeschlagenen Login senden (falsches Passwort)
        std::cout << "[ERROR] Login failed for user " << name << std::endl;
    }
}

// Funktion zum Ausloggen eines Benutzers
void handleLogout(SOCKET clientSocket) {
    for (auto& pair : registeredUsers) {
        User& user = pair.second;
        if (user.socket == clientSocket && user.loggedIn) {  // Benutzer finden, der ausgeloggt werden soll
            user.loggedIn = false;  // Als ausgeloggt markieren
            user.currentSession = -1;  // Sitzung zurücksetzen
            std::cout << "[INFO] User " << user.name << " logged out successfully." << std::endl;
            sendMessage(clientSocket, 104);  // Erfolgreiches Ausloggen bestätigen
            return;
        }
    }
    std::cout << "[ERROR] Logout failed: user not found or already logged out." << std::endl;
}

// Funktion zum Senden der Sitzungsliste an den Client
void handleSessionListRequest(SOCKET clientSocket) {
    std::vector<char> sessionListData;
    for (const auto& session : activeSessions) {
        sessionListData.insert(sessionListData.end(), (char*)&session.second.sessionNumber, (char*)&session.second.sessionNumber + 4);  // Sitzungsnummer hinzufügen
        sessionListData.push_back(static_cast<char>(session.second.player2.empty() ? 0 : session.second.player2.size()));  // Länge des Gegenspielernamens
        sessionListData.push_back(static_cast<char>(session.second.passwordProtected));  // Passwortschutz hinzufügen
    }
    sendMessage(clientSocket, 103, sessionListData);  // Sitzungsliste an den Client senden
    std::cout << "[INFO] Sent session list with " << activeSessions.size() << " active sessions." << std::endl;
}

// Funktion zum Erstellen einer neuen Sitzung
void handleCreateSession(SOCKET clientSocket, const std::vector<char>& message) {
    int passwordLength = message[5];  // Länge des Sitzungspassworts
    std::string password(message.begin() + 6, message.begin() + 6 + passwordLength);  // Sitzungspasswort
    //system("pause");
    // Überprüfung, ob die maximale Anzahl an Sitzungen erreicht ist
    if (activeSessions.size() >= SESSION_LIMIT) {
        std::vector<char> denyData = { 4 };  // Sitzungslimit erreicht
        sendMessage(clientSocket, 112, denyData);  // Zugriff verweigern
        std::cout << "[ERROR] Session limit reached, can't create new session." << std::endl;
        return;
    }

    // Sitzungszähler erhöhen
    sessionCounter++;
    std::string playerName;

    // Suche nach dem Namen des Spielers, der die Sitzung erstellt
    for (const auto& pair : registeredUsers) {
        const std::string& name = pair.first;
        const User& user = pair.second;

        if (user.socket == clientSocket && user.loggedIn) {
            playerName = name;
            break;
        }
    }

    // Neue Sitzung erstellen und speichern
    Session newSession = { sessionCounter, playerName, "", !password.empty(), password };
    activeSessions[sessionCounter] = newSession;

    // Antwort an den Client vorbereiten (Erfolgsmeldung)
    std::vector<char> accessData(13, 0);
    *(int*)accessData.data() = sessionCounter;  // Sitzungsnummer
    accessData[4] = 0;  // Noch kein Gegenspieler
    accessData[5] = 1;  // Spieler 1 ist am Zug
    std::fill(accessData.begin() + 6, accessData.end(), '-');  // Spielfeld initialisieren

    // Erfolgreiche Sitzungsfreigabe an den Client senden
    sendMessage(clientSocket, 108, accessData);

    std::cout << "[SUCCESS] Created new session with ID: " << sessionCounter << std::endl;
}

void handleJoinSession(SOCKET clientSocket, const std::vector<char>& message) {
    int sessionNumber = message[5];  // Sitzungsnummer auslesen (assuming it starts at message[1])
    int passwordLength = message[6];  // Länge des Passworts

    std::cout << "[INFO] session num " << sessionNumber << std::endl;
    std::cout << "[INFO] pw len " << passwordLength << std::endl;
    
    std::string sessionPassword(message.begin() + 7, message.begin() + 7 + passwordLength);  // Passwort

    // Suche nach dem Namen des Spielers, der die Sitzung beitreten möchte
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

    // Überprüfen, ob die Sitzung existiert
    auto sessionIt = activeSessions.find(sessionNumber);
    if (sessionIt == activeSessions.end()) {
        std::vector<char> denyData = { 0 };  // Game does not exist
        sendMessage(clientSocket, 112, denyData);  // Zugriff verweigern
        std::cout << "[ERROR] Session " << sessionNumber << " does not exist." << std::endl;
        return;
    }

    Session& session = sessionIt->second;

    // Überprüfen, ob die Sitzung bereits voll ist
    if (!session.player2.empty()) {
        std::vector<char> denyData = { 1 };  // Game is full
        sendMessage(clientSocket, 112, denyData);  // Zugriff verweigern
        std::cout << "[ERROR] Session " << sessionNumber << " is full." << std::endl;
        return;
    }

    // Überprüfen, ob das Passwort korrekt ist (falls passwortgeschützt)
    if (session.passwordProtected && session.password != sessionPassword) {
        std::vector<char> denyData = { 2 };  // Wrong password
        sendMessage(clientSocket, 112, denyData);  // Zugriff verweigern
        std::cout << "[ERROR] Incorrect password for session " << sessionNumber << "." << std::endl;
        return;
    }

    // Überprüfen, ob der Spieler bereits in der Sitzung ist
    if (session.player1 == playerName || session.player2 == playerName) {
        std::vector<char> denyData = { 3 };  // Already in session
        sendMessage(clientSocket, 112, denyData);  // Zugriff verweigern
        std::cout << "[ERROR] User " << playerName << " is already in session " << sessionNumber << "." << std::endl;
        return;
    }

    // Benutzer als zweiten Spieler in der Sitzung hinzufügen
    session.player2 = playerName;

    // Erfolgreiche Sitzungsfreigabe an den Client senden
    //std::vector<char> accessData(14 + session.player1.size() + 9, 0);  // Reserve space for the game board
    //*(int*)accessData.data() = sessionNumber;  // Sitzungsnummer
    //accessData[4] = static_cast<char>(session.player1.size());  // Länge des Spielernamens
    //std::copy(session.player1.begin(), session.player1.end(), accessData.begin() + 5);  // Name des ersten Spielers hinzufügen
    //accessData[5 + session.player1.size()] = session.isPlayer1Turn ? 1 : 0;  // Ist Spieler 1 an der Reihe?

    // Spielfeldzustand hinzufügen
    //std::copy(session.board, session.board + 9, accessData.begin() + 6 + session.player1.size());

    sendMessage(clientSocket, 108);//, accessData);  // Erfolgreiche Sitzungsfreigabe senden
    std::cout << "[SUCCESS] User " << playerName << " joined session " << sessionNumber << "." << std::endl;
}



// Funktion zur Verarbeitung von Nachrichten vom Client
void processClientMessage(SOCKET clientSocket, const std::vector<char>& message) {
    int messageCode = message[4];  // Nachrichtencode auslesen
    switch (messageCode) {
    case 1:
        handleRegister(clientSocket, message);  // Registrierung
        break;
    case 3:
        handleLogin(clientSocket, message);  // Login
        break;
    case 4:
        handleLogout(clientSocket);  // Logout
        break;
    case 2:
        handleSessionListRequest(clientSocket);  // Sitzungsliste anfordern
        break;
    case 5:
        handleCreateSession(clientSocket, message);  // Sitzung erstellen
        break;
    case 6:
        handleJoinSession(clientSocket, message);  // Sitzung erstellen
        break;
    default:
        std::cout << "[ERROR] Unknown message code: " << messageCode << std::endl;
    }
}

// Funktion zur Verarbeitung von Nachrichten aller Clients
void handleClientMessages() {
    for (auto& client : clientBuffers) {
        SOCKET clientSocket = client.first;
        std::vector<char>& buffer = client.second;

        while (buffer.size() >= 5) {  // Solange Nachrichten vorhanden sind
            int messageLength = *(int*)buffer.data();  // Nachrichtengröße auslesen
            if (buffer.size() >= messageLength + 4) {
                std::vector<char> message(buffer.begin(), buffer.begin() + messageLength + 4);  // Nachricht extrahieren
                int messageCode = message[4];
                std::cout << "[INFO] processing message with code: " << messageCode << std::endl;
                processClientMessage(clientSocket, message);  // Nachricht verarbeiten
                buffer.erase(buffer.begin(), buffer.begin() + messageLength + 4);  // Nachricht aus dem Puffer entfernen
            }
            else {
                break;
            }
        }
    }
}

int main() {
    initWinsock();  // Winsock initialisieren
    SOCKET listener = createListenerSocket();  // Listener-Socket erstellen

    fd_set master;
    FD_ZERO(&master);
    FD_SET(listener, &master);  // Listener-Socket zu fd_set hinzufügen

    while (true) {
        fd_set copy = master;
        int socketCount = select(0, &copy, nullptr, nullptr, nullptr);  // Auf eingehende Verbindungen warten

        for (int i = 0; i < socketCount; i++) {
            SOCKET sock = copy.fd_array[i];

            if (sock == listener) {  // Neue Verbindung annehmen
                SOCKET client = accept(listener, nullptr, nullptr);
                FD_SET(client, &master);  // Client-Socket zu fd_set hinzufügen
                clientBuffers[client] = std::vector<char>();  // Nachrichtenpuffer für den neuen Client erstellen
                std::cout << "[INFO] New client connected." << std::endl;
            }
            else {
                char buf[BUFFER_SIZE];
                int bytesReceived = recv(sock, buf, BUFFER_SIZE, 0);  // Nachricht empfangen

                if (bytesReceived <= 0) {  // Client hat die Verbindung geschlossen
                    closesocket(sock);
                    FD_CLR(sock, &master);  // Client-Socket aus fd_set entfernen
                    clientBuffers.erase(sock);  // Nachrichtenpuffer löschen
                    std::cout << "[INFO] Client disconnected." << std::endl;
                }
                else {
                    std::vector<char>& clientBuffer = clientBuffers[sock];
                    clientBuffer.insert(clientBuffer.end(), buf, buf + bytesReceived);  // Nachricht zum Puffer hinzufügen
                }
            }
        }

        handleClientMessages();  // Nachrichten aller Clients verarbeiten
    }

    FD_CLR(listener, &master);
    closesocket(listener);  // Listener-Socket schließen
    WSACleanup();  // Winsock aufräumen
    return 0;
}
