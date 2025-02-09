#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

#pragma comment(lib, "Ws2_32.lib")



std::unordered_map<SOCKET, std::string> active_users; //Store currently online users' name and the corresponding sockets

SOCKET FindSocketByUsername(const std::string& username) {
    for (const auto& pair : active_users) {
        if (pair.second == username) {
            return pair.first;  // Find corresponding socket by the username
        }
    }
    return INVALID_SOCKET;  // Return invalid SOCKET if not found
}

//Function for sending a DM message, the DM messages sent by server always starts with a "#" for recognization
void Private_send(SOCKET target_socket, std::string sender, std::string message) {
    message = "#" + sender + ":" + message;
    send(target_socket, message.c_str(), static_cast<int>(message.size()), 0);
}

//When a user disconnected, broadcast the deletion message to all the current users
void Broadcast_delete(std::string dname) {
    for (auto& s : active_users) { //deletion message will be broadcast to all users within the active_user map
        SOCKET socket = s.first;
        send(socket, dname.c_str(), static_cast<int>(dname.size()), 0);
    }
}

//When a new user join the chatroom, broadcast a join message to all the users. A join message starts with a "+".
//Then broadcast the current active user list to the new user
void Broadcast_user(SOCKET current_socket, std::string myName) {
    for (auto& s : active_users) {
        SOCKET socket = s.first;
        std::string name = s.second;
        name = "+" + name;
        send(current_socket, name.c_str(), static_cast<int>(name.size()), 0);//send the curretly online user to the new user

        myName = "+" + myName;
        send(socket, myName.c_str(), static_cast<int>(myName.size()), 0); //send the new user to a currently online user
        std::cout << "Pushed name: " << myName << "\n";
    }
}

//the function to broadcast a message when a user send something in the public chatroom
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

//Handles the connection with each client, when receiving a message, determine what kind of action this message indicate,
//and process the action
void client_connection(SOCKET client_socket, int id) {
    //When a new client connect with the server, the client will automatically send a message containing the username.
    //Therefore the first message the server recieved will be the username of the new client
    char buffer[1024] = { 0 };
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    std::cout << "User " << buffer << " joined!" << "\n";
    Broadcast_user(client_socket, buffer); //Broadcast the new client
    active_users[client_socket] = buffer; //Store the new client's name and the corresponding socket
    memset(buffer, 0, sizeof(buffer)); //reset the buffer

    // Step 6: Communicate with the client
    bool stop = false;
    while (!stop) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            if (buffer[0] == '-') { //If the first charactor of a message is "-", it means a client just disconnect from server
                active_users.erase(client_socket); //erase the user from active user list
                closesocket(client_socket); //close the socket
                Broadcast_delete(buffer); //broadcast the deletion to rest of clients
                std::cout << buffer << " disconnected!" << "\n";
            }
            //If the first charactor of a message is "#", it means it is a private message
            //Whenever a client sends a private message, it will first send the target client's name with a "#" in front of the message
            //then another message with the content of the private message will be sent
            else if (buffer[0] == '#') { 
                std::string target_user = buffer + 1; // remove the "#" in the message
                char nb[1024] = { 0 };
                int bytes = recv(client_socket, nb, sizeof(nb) - 1, 0); //receive the DM content
                SOCKET target_socket = FindSocketByUsername(target_user); //find the socket corresponding to the target username
                Private_send(target_socket, active_users[client_socket], nb); //sent the private message
                std::cout << "Received DM from " + active_users[client_socket] << ", send to " << target_user << ": " << nb << "\n";
            }
            else { //Otherwise it is a message sent to public chatroom, so broadcast the message to all clients
                Broadcast_message(client_socket, buffer);
            }
            std::cout << "Received(" << id << "): " << buffer << std::endl;

            memset(buffer, 0, sizeof(buffer)); //reset the buffer
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