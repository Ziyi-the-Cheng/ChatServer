#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

#pragma comment(lib, "Ws2_32.lib")



std::unordered_map<SOCKET, std::string> active_users;

SOCKET FindSocketByUsername(const std::string& username) {
    for (const auto& pair : active_users) {
        if (pair.second == username) {
            return pair.first;  // 找到对应的 SOCKET 并返回
        }
    }
    return INVALID_SOCKET;  // 如果没找到，返回无效 SOCKET
}

void Private_send(SOCKET target_socket, std::string sender, std::string message) {
    message = "#" + sender + ":" + message;
    send(target_socket, message.c_str(), static_cast<int>(message.size()), 0);
}

void Broadcast_delete(std::string dname) {
    for (auto& s : active_users) {
        SOCKET socket = s.first;
        send(socket, dname.c_str(), static_cast<int>(dname.size()), 0);
    }
}

void Broadcast_user(SOCKET current_socket, std::string myName) {
    for (auto& s : active_users) {
        SOCKET socket = s.first;
        std::string name = s.second;
        name = "+" + name;
        send(current_socket, name.c_str(), static_cast<int>(name.size()), 0);

        myName = "+" + myName;
        send(socket, myName.c_str(), static_cast<int>(myName.size()), 0);
        std::cout << "Pushed name: " << myName << "\n";
    }
}

void Broadcast_message(SOCKET current_socket, std::string message) {
    message = active_users[current_socket] + ": " + message;
    for (auto& s : active_users) {
        SOCKET socket = s.first;
        if (socket != current_socket) {
            send(socket, message.c_str(), static_cast<int>(message.size()), 0);
            std::cout << "Broadcast message: " << message << "\n";
        }
    }
}

void client_connection(SOCKET client_socket, int id) {
    char buffer[1024] = { 0 };
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    std::cout << "User " << buffer << " joined!" << "\n";
    Broadcast_user(client_socket, buffer);
    active_users[client_socket] = buffer;
    memset(buffer, 0, sizeof(buffer));

    // Step 6: Communicate with the client
    bool stop = false;
    while (!stop) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            if (buffer[0] == '-') {
                active_users.erase(client_socket);
                closesocket(client_socket);
                Broadcast_delete(buffer);
                std::cout << buffer << " disconnected!" << "\n";
            }
            else if (buffer[0] == '#') {
                std::string target_user = buffer + 1;
                char nb[1024] = { 0 };
                int bytes = recv(client_socket, nb, sizeof(nb) - 1, 0);
                SOCKET target_socket = FindSocketByUsername(target_user);
                Private_send(target_socket, active_users[client_socket], nb);
                std::cout << "Received DM from " + active_users[client_socket] << ", send to " << target_user << ": " << nb << "\n";
            }
            else {
                Broadcast_message(client_socket, buffer);
            }
            std::cout << "Received(" << id << "): " << buffer << std::endl;

            memset(buffer, 0, sizeof(buffer));
        }
    }
    // Step 7: Clean up
    closesocket(client_socket);
}

int server_loop_multi() {
    // Step 1: Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Step 2: Create a socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Step 3: Bind the socket
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(65432);  // Server port
    server_address.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Step 4: Listen for incoming connections
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 65432..." << std::endl;

    int connections = 0;
    while (true) {
        // Step 5: Accept a connection
        sockaddr_in client_address = {};
        int client_address_len = sizeof(client_address);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return 1;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;

        //std::thread* t = new std::thread(client_connection, client_socket, ++connections);
        std::thread t = std::thread(client_connection, client_socket, ++connections);
        t.detach();
    }
    closesocket(server_socket);
    WSACleanup();

    return 0;
}

int main() {
    
    server_loop_multi();
}