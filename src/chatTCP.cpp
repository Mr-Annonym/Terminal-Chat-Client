#include <iostream>
#include <string>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include <sys/socket.h>
#include "chat.hpp"

// Constructor for ChatTCP
ChatTCP::ChatTCP(NetworkAdress& receiver) : Chat(receiver) {
    client.displayName = "unknown"; // default value
    tcpFactory = new TCPMessages(&client);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cout << "Error opening socket\n";
        destruct();
        exit(1);
    }

    sockaddr_in server = this->receiver;

    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        std::cout << "Connection failed\n";
        perror("connect");
        destruct();
        exit(1);
    }

    setNonBlocking(sockfd);
}

void ChatTCP::handleIncommingMessage(Message* message) {
    if (message == nullptr) handleDisconnect();

    // check, if the response is allowed
    if (!msgTypeValidForStateReceived(message->getType())) {
        std::string errMsg;
        if (state == FSMState::AUTH) {
            errMsg = "Invalid message, expected REPLY but go MESSAGE";
        } else if (state == FSMState::OPEN) {
            errMsg = "Invalid message, expected MESSAGE but got REPLY";
        }
        MessageErrorTCP* errorMessage = new MessageErrorTCP(client.displayName, errMsg);
        backendSendMessage(errorMessage->getMessage());
        handleDisconnect();
        return;
    } 
    
    // user Message
    if (typeid(*message) == typeid(MessageMsgTCP) || typeid(*message) == typeid(MessageMsgUDP)) {
        printMessage(message);
        return;
    }

    // Repaly
    printStatusMessage(message);
}

// Destructor for ChatTCP (closing the sockets)
void ChatTCP::destruct() {
    deleteBuffer();
    if (sockfd >= 0) {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
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
            std::cout << "ERROR: you need to authenticate first\n";
            return;
        }
        std::cout << "ERROR: invalid input, try again or seek /help\n";
        return;
    }

    backendSendMessage(message->getMessage());

    // if we sent an auth message or join message, we need to wait for a reply
    MessageType msgType = message->getType();
    if (msgType != MessageType::AUTH && msgType != MessageType::JOIN) return;

    // wait for a reply
    waitForResponseWithTimeout();
}

void ChatTCP::waitForResponseWithTimeout() {

    int timeLeft = timeout_ms;
    std::string responseOut = waitForResponse(&timeLeft);
    // no response -> timeout
    if (responseOut.empty()) {
        std::cout << "ERROR: timeout on message recv\n";
        sendTimeoutErrMessage();
        handleDisconnect();
        return;
    }

    // handle the msg
    Message* msg = parseResponse(responseOut);
    handleIncommingMessage(msg);
}

// method for parsing the server response
Message* ChatTCP::parseResponse(std::string response) {
    return tcpFactory->readResponse(response);
}

// method for handling disconnection
void ChatTCP::handleDisconnect() {
    sendByeMessage();
    destruct();
    exit(1); // err code .. 
}

// method for sending a BYE message
void ChatTCP::sendByeMessage() {
    MessageByeTCP* byMessage = new MessageByeTCP(client.displayName);
    backendSendMessage(byMessage->getMessage());
    destruct();
}

void ChatTCP::sendTimeoutErrMessage() {
    MessageErrorTCP* errMessage = new MessageErrorTCP(client.displayName, "Timeout while waiting for server response");
    backendSendMessage(errMessage->getMessage());
}

// method for receiving server response
std::string ChatTCP::backendGetServerResponse() {
    // Clear the buffer first
    memset(this->buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(sockfd, this->buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0 || bytes_received == 0) {
        std::cout << "ERROR: receiving message or connection closed\n";        
        handleDisconnect();
    }
    return std::string(this->buffer, bytes_received);
}

// method for sending message to server
void ChatTCP::backendSendMessage(std::string message) {
    size_t total_sent = 0;
    size_t message_length = message.size();
    while (total_sent < message_length) {
        ssize_t bytes_sent = send(sockfd, message.c_str() + total_sent, message_length - total_sent, 0);
        if (bytes_sent < 0) {
            std::cout << "Error sending message\n";
            exit(1);
        }
        total_sent += bytes_sent;
    }
}
