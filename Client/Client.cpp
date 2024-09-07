#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdlib>

#pragma comment(lib, "Ws2_32.lib")

void clearConsole() {
    system("cls");
}

void setRecvTimeout(SOCKET clientSocket, int timeoutSec) {
    int timeoutMs = timeoutSec * 1000;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));
}

void sendMove(SOCKET& clientSocket, int sessionId, int row, int col) {
    std::string message = "MOVE:" + std::to_string(sessionId) + ":" + std::to_string(row) + ":" + std::to_string(col);
    send(clientSocket, message.c_str(), message.size() + 1, 0);
}

void registerUser(SOCKET& clientSocket, const std::string& username, const std::string& password) {
    std::string message = "REGISTER:" + username + ":" + password;
    send(clientSocket, message.c_str(), message.size() + 1, 0);
}

void loginUser(SOCKET& clientSocket, const std::string& username, const std::string& password) {
    std::string message = "LOGIN:" + username + ":" + password;
    send(clientSocket, message.c_str(), message.size() + 1, 0);
}

int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverHint;
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(54000);
    inet_pton(AF_INET, "127.0.0.1", &serverHint.sin_addr);

    iResult = connect(clientSocket, (sockaddr*)&serverHint, sizeof(serverHint));
    if (iResult == SOCKET_ERROR) {
        std::cout << "Connect failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::string action, username, password;
    std::cout << "Möchtest du dich registrieren oder einloggen? (registrieren/einloggen): ";
    std::cin >> action;

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

    char buffer[512];
    ZeroMemory(buffer, 512);
    iResult = recv(clientSocket, buffer, 512, 0);
    if (iResult > 0) {
        std::cout << "Server: " << buffer << std::endl;
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
