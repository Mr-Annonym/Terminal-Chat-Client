/**
 * @file chatTCP.cpp
 * @brief Implementation of the ChatTCP class
 * @author Martin Mendl <x247581>
 * @date 18.4.2025
*/

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include <sys/socket.h>
#include <cstring>
#include <cstring>
#include "chat.hpp"

// Constructor for ChatTCP
ChatTCP::ChatTCP(NetworkAdress& receiver) : Chat(receiver) {
    client.displayName = "unknown"; // default value
    tcpFactory = new TCPMessages(&client); // factory for TCP messages

    // create the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cout << "Error opening socket\n" << std::flush;;
        destruct();
        exit(1);
    }

    sockaddr_in server = this->receiver;

    // connect to the server
    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        std::cout << "Connection failed\n" << std::flush;;
        perror("connect");
        destruct();
        exit(1);
    }

    // create a buffer for the current message, since I am using stream on socket, meaning messges can be split
    currentMessage.reserve(BUFFER_SIZE); 

    // set the socket to non-blocking mode because I am using poll
    setNonBlocking(sockfd);
}

// Method to read an incoming message from the server
void ChatTCP::readMessageFromServer() {

    // get the server response
    std::string response = backendGetServerResponse();

    /*
    Basically here, i know that each message is terminated by \r\n
    so once a messgae comes in, I will parse it, if i find \r\n, I will
    add the message to the buffer and clear the current message
    if the message is not terminated, I will just add it to the current message
    */
    char currentChar = '\0';
    char prevChar = (currentMessage.empty()) ? '\0' : currentMessage.back();
    for (unsigned i = 0; i < response.size(); i++) {
        currentChar = response[i];
        currentMessage += currentChar;

        // we have a complete message
        if (currentChar == '\n' && prevChar == '\r') {
            msgBuffer->addMessage(currentMessage);
            currentMessage.clear();
        }
        
        prevChar = currentChar;
    }

    // handle incoming messages
    while (!msgBuffer->isEmpty()) {
        std::string message = msgBuffer->getMessage();
        if (message.empty()) continue;

        // parse the message
        Message* msg = parseResponse(message);
        // handle the message
        handleIncommingMessage(msg);
    }
}

// Method to handle an incoming message
void ChatTCP::handleIncommingMessage(Message* message) {

    // if the message is nullptr, it means that we dont understand the message or it is malformed
    if (message == nullptr) {
        std::cout << "ERROR: invalid message/malformed message\n" << std::flush;
        MessageErrorTCP* errMsg = new MessageErrorTCP(client.displayName, "invalid message/malformed message");
        backendSendMessage(errMsg->getMessage());
        handleDisconnect();
    }

    // if the message is an error message, print it and disconnect
    if (message->getType() == MessageType::ERR) {
        MessageErrorTCP* errorMessage = dynamic_cast<MessageErrorTCP*>(message);
        if (!errorMessage){
            std::cerr << "faild to dynamic cast message to error message\n" << std::flush;
            exit(1);
        }
        std::cout << "ERROR FROM " << errorMessage->getDisplayName() << ": " << errorMessage->getContent() << std::endl << std::flush;
        handleDisconnect();
        return;
    }

    // check, if the response is allowed by the FSM
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
    if (typeid(*message) == typeid(MessageMsgTCP)) {
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
        shutdown(sockfd, SHUT_RDWR); // to not invoke the RST msg on the server
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

    // parse the user input
    Command* command = handleUserInput(userInput);
    // invalid user input
    if (command == nullptr) return; 

    // create the message, form the user Command
    Message* message = tcpFactory->convertCommandToMessage(command);

    // check if the message is valid
    if (message == nullptr || !msgTypeValidForStateSent(message->getType())) {
        // dont exit program here, this is user error
        if (state == FSMState::START) {
            std::cout << "ERROR: you need to authenticate first\n" << std::flush;;
            return;
        }
        std::cout << "ERROR: invalid input, try again or seek /help\n" << std::flush;;
        return;
    }

    // send the message to the server
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
        std::cout << "ERROR: timeout on message recv\n" << std::flush;;
        sendTimeoutErrMessage();
        handleDisconnect();
        return;
    }

    // parse the response
    Message* msg = parseResponse(responseOut);
    // handle the response
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
    exit(0);
}

// method for sending a BYE message
void ChatTCP::sendByeMessage() {
    MessageByeTCP* byMessage = new MessageByeTCP(client.displayName);
    backendSendMessage(byMessage->getMessage());
    destruct();
}

// method for sending a timeout error message
void ChatTCP::sendTimeoutErrMessage() {
    MessageErrorTCP* errMessage = new MessageErrorTCP(client.displayName, "Timeout while waiting for server response");
    backendSendMessage(errMessage->getMessage());
}

// method for receiving server response
std::string ChatTCP::backendGetServerResponse() {
    // Clear the buffer first
    memset(this->buffer, 0, BUFFER_SIZE);

    // Receive the message from the server
    int bytes_received = recv(sockfd, this->buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0 || bytes_received == 0) {
        std::cout << "ERROR: receiving message or connection closed\n" << std::flush;        
        handleDisconnect();
    }
    return std::string(this->buffer, bytes_received);
}

// method for sending message to server
void ChatTCP::backendSendMessage(std::string message) {
    size_t total_sent = 0;
    size_t message_length = message.size();

    // Send the message in chunks
    while (total_sent < message_length) {
        ssize_t bytes_sent = send(sockfd, message.c_str() + total_sent, message_length - total_sent, 0);
        if (bytes_sent < 0) {
            std::cout << "Error sending message\n" << std::flush;;
            exit(1);
        }
        total_sent += bytes_sent;
    }
}
