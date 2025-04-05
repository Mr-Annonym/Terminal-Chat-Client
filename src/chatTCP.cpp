#include <iostream>
#include <string>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <csignal>
#include <sys/socket.h>
#include "chat.hpp"

// Constructor for ChatTCP
ChatTCP::ChatTCP(NetworkAdress& receiver) : Chat(receiver) {
    client.displayName = "unknown"; // default value
    tcpFactory = new TCPMessages(&client);

    sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd1 < 0) {
        std::cerr << "Error opening socket\n";
        destruct();
        exit(1);
    }

    sockaddr_in server = this->receiver;

    if (connect(sockfd1, (struct sockaddr *)&server, sizeof(server)) < 0) {
        std::cerr << "Connection failed\n";
        perror("connect");
        destruct();
        exit(1);
    }

    setNonBlocking(sockfd1);
}

// Destructor for ChatTCP (closing the sockets)
void ChatTCP::destruct() {
    deleteBuffer();
    if (sockfd1 >= 0) {
        shutdown(sockfd1, SHUT_RDWR);
        close(sockfd1);
        sockfd1 = -1;
    }
}

// Destructor for ChatTCP
ChatTCP::~ChatTCP() {
    destruct();
}

// method for sending message to server
void ChatTCP::sendMessage(std::string userInput) {
    Command* command = handleUserInput(userInput);
    if (command == nullptr) return;

    Message* message = tcpFactory->convertCommandToMessage(command);
    if (message == nullptr || !msgTypeValidForStateSent(message->getType())) {
        if (state == FSMState::START) {
            std::cerr << "You need to authenticate first\n";
            return;
        }
        std::cerr << "Invalid input try again\n";
        return;
    }

    std::string msg = message->getMessage();
    backendSendMessage(msg);

    // if we sent an auth message or join message, we need to wait for a reply
    MessageType msgType = message->getType();
    if (msgType != MessageType::AUTH && msgType != MessageType::JOIN) return;

    // wait for a reply
    waitForResponseWithTimeout();
}

// method for sending message to server
void ChatTCP::backendSendMessage(std::string message) {
    size_t total_sent = 0;
    size_t message_length = message.size();
    while (total_sent < message_length) {
        ssize_t bytes_sent = send(sockfd1, message.c_str() + total_sent, message_length - total_sent, 0);
        if (bytes_sent < 0) {
            std::cerr << "Error sending message\n";
            exit(1);
        }
        total_sent += bytes_sent;
    }
}

void ChatTCP::waitForResponseWithTimeout() {
    struct pollfd pfd;
    pfd.fd = sockfd1;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        std::string responseOut = getServerResponse();
        Message* message = parseResponse(responseOut);
        handleIncommingMessage(message);
        return;
    } 

    // error
    if (ret < 0) {
        std::cerr << "Internal error: poll failed\n";
        handleDisconnect();
    }

    // timeout
    std::cerr << "Timeout waiting for server response\n";
    handleDisconnect();
}


// method for receiving server response
std::string ChatTCP::getServerResponse() {
    char buffer[1024] = {0};
    int bytes_received = recv(sockfd1, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0 || bytes_received == 0) {
        std::cerr << "Error receiving message or connection closed\n";        
        handleDisconnect();
    }
    return std::string(buffer, bytes_received);
}

// method for parsing the server response
Message* ChatTCP::parseResponse(std::string response) {
    return tcpFactory->readResponse(response);
}

// method for handling disconnection
void ChatTCP::handleDisconnect() {
    std::cout << "\nDisconnecting...\n";
    sendByeMessage(); // no user name yet
    destruct();
    exit(1); // err code .. 
}

// method for sending a BYE message
void ChatTCP::sendByeMessage() {
    MessageByeTCP* byMessage = new MessageByeTCP(client.displayName);
    backendSendMessage(byMessage->getMessage());
    destruct();
}
