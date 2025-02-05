#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

#pragma comment(lib, "Ws2_32.lib")



std::unordered_map<SOCKET, std::string> active_users;


void Broadcast(SOCKET current_socket) {
    for (auto& s : active_users) {
        SOCKET socket = s.first;
        std::string name = s.second;
        if (socket != current_socket)
            name = "+" + name;
        send(socket, name.c_str(), static_cast<int>(name.size()), 0);

    }
}


void client_connection(SOCKET client_socket, int id) {
    char buffer[1024] = { 0 };
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    active_users[client_socket] = buffer;
    std::cout << "User " << buffer << " joined!";
    Broadcast(client_socket);

    // Step 6: Communicate with the client
    bool stop = false;
    while (!stop) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::cout << "Received(" << id << "): " << buffer << std::endl;

            // Reverse the string
            std::string response(buffer);
            if (client_socket == INVALID_SOCKET) {
                stop = true;
                std::cout << "User " << active_users[client_socket] << " disconnected!";
            } 
            std::reverse(response.begin(), response.end());

            // Send the reversed string back
            send(client_socket, response.c_str(), static_cast<int>(response.size()), 0);
            std::cout << "Reversed string sent back to client " << id << std::endl;
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