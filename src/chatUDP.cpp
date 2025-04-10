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

ChatUDP::ChatUDP(NetworkAdress& receiver, int retransmissions, int timeout) : Chat(receiver) {
    client.displayName = "unknown"; // can be changed later
    this->retransmissions = retransmissions;
    this->timeout = timeout;

    udpFactory = new UDPMessages(&client);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cout << "ERROR: faild to create a socket\n";
        destruct();
        exit(1);
    }

    // Set the socket to non-blocking mode
    setNonBlocking(sockfd);
}

// Destructor for ChatUDP
ChatUDP::~ChatUDP() {
    destruct();
}

// method for ending the communication with the server
void ChatUDP::destruct() {
    deleteBuffer();
    if (sockfd >= 0) {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
    }
}

void ChatUDP::readMessageFromServer() {
    std::string response = backendGetServerResponse();
    Message* message = parseResponse(response);
    handleIncommingMessage(message);
}

Message* ChatUDP::parseResponse(std::string response) {
    // parse the response and return the message object
    return udpFactory->readResponse(response);;
}

// gets the command, creates the message obj and calls backendSendMessage
void ChatUDP::sendMessage(std::string userInput) {

    // first send the message
    Command* command = handleUserInput(userInput);
    if (command == nullptr) return;

    Message* msg = udpFactory->convertCommandToMessage(msgCount, command);
    if (msg == nullptr || !msgTypeValidForStateSent(msg->getType())) {
        if (state == FSMState::START) {
            std::cout << "ERROR: you authenticate first\n";
            return;
        }
        std::cout << "ERROR: invalid input, try again or use /help\n";
        return;
    }

    // handle retransmitions
    for (int attempt = 0; attempt < retransmissions; ++attempt) {
        // send the message
        backendSendMessage(msg->getMessage());
        MessageType type = msg->getType();
    
        // wait for confirmation or if it is JOIN or AUTH message call the confirmation thing
        if (type == MessageType::JOIN || type == MessageType::AUTH) {
            waitForResponseWithTimeout(msg);
            msgCount++;
            return;
        }
        
        // msg/err/bye message
        if (waitForConfirmation(msg)) {
            msgCount++;
            return; // we good
        }
    }
    handleDisconnect();
};

bool ChatUDP::waitForConfirmation(Message* msg) {

    int timeLeft = this->timeout;
    uint16_t msgID = msg->getId();

    while (timeLeft > 0) {
        std::string responseOut = waitForResponse(&timeLeft);
        // no response -> timeout
        if (responseOut.empty()) continue;
        // not a valid response
        Message* message = parseResponse(responseOut);
        if (message == nullptr) continue;        

        // got a ping message
        if (message->getType() == MessageType::PING) {
            uint16_t msgId = message->getId();
            MessageConfirm* msg = new MessageConfirm(msgId);
            backendSendMessage(msg->getMessage());
            continue;
        }   

        if (message->getType() != MessageType::CONFIRM) continue;
        if (msgID != message->getId()) continue;
        return true;
    }
    std::cout << "ERROR: timeout on message recv\n";
    return false;
}

// when this receaves a msg, it will send a comfirm back (mby not needed) 
void ChatUDP::waitForResponseWithTimeout(Message* msg) {

    int timeLeft = timeout_ms;
    uint16_t msgID = msg->getId();

    while (timeLeft > 0) {
        std::string responseOut = waitForResponse(&timeLeft);
        // timeout
        if (responseOut.empty()) {
            std::cout << "ERROR: timeout on message recv\n";
            sendTimeoutErrMessage();
            handleDisconnect();
            return;
        }

        Message* message = parseResponse(responseOut);

        // not a valid response
        if (message == nullptr) {
            std::cout << "ERROR: invalid message received\n";
            handleDisconnect();
            return;
        }
        
        MessageType type = message->getType();

        // ping message
        if (type == MessageType::PING) {
            uint16_t msgId = message->getId();
            MessageConfirm* msg = new MessageConfirm(msgId);
            backendSendMessage(msg->getMessage());
            continue;
        }

        if (type == MessageType::MSG) {
            // send a confirmation message
            uint16_t msgId = message->getId();
            MessageConfirm* msg = new MessageConfirm(msgId);
            backendSendMessage(msg->getMessage());
            // print the message
            printMessage(message);
            continue;
        }

        // confirmation message
        if (type == MessageType::CONFIRM) continue;
        if (type != MessageType::pREPLY && type != MessageType::nREPLY) continue;
        
        // reply to msg
        if (msgID != msg->getId()) { // mby continue here?
            std::cout << "ERROR: invalid message received\n";
            handleDisconnect();
            continue;
        }
        
        // we have a good reply
        // dynamic switching, after auth
        if (msg->getType() == MessageType::AUTH && state == FSMState::AUTH) {
            receiver.sin_port = sender_addr.sin_port; 
        }

        handleIncommingMessage(message);
        return;
    }

    std::cout << "ERROR: timeout on message recv\n";
    sendTimeoutErrMessage();
    handleDisconnect();
    return;
};

// method for handling incoming messages
void ChatUDP::handleIncommingMessage(Message* message) {
    if (message == nullptr) handleDisconnect();

    if (message->getType() == MessageType::PING) {
        uint16_t msgId = message->getId();
        MessageConfirm* msg = new MessageConfirm(msgId);
        backendSendMessage(msg->getMessage());
        return;
    }

    // first, check if the response is allowed
    if (!msgTypeValidForStateReceived(message->getType())) {
        std::string errMsg;
        if (state == FSMState::AUTH) {
            errMsg = "Invalid message, expected REPLY but go MESSAGE";
        } else if (state == FSMState::OPEN) {
            errMsg = "Invalid message, expected MESSAGE but got REPLY";
        }
        MessageErrorUDP* errorMessage = new MessageErrorUDP(msgCount, client.displayName, errMsg);
        backendSendMessage(errorMessage->getMessage());
        handleDisconnect();
        return;
    }

    // second, we send a confirmation message back to the server
    if (message->getType() != MessageType::CONFIRM) {
        MessageConfirm* msg = new MessageConfirm(message->getId());
        backendSendMessage(msg->getMessage());
    }

    // user Message
    if (typeid(*message) == typeid(MessageMsgTCP) || typeid(*message) == typeid(MessageMsgUDP)) {
        printMessage(message);
        return;
    }

    // Repaly
    printStatusMessage(message);
}

// send the bye message and closes stuff
void ChatUDP::handleDisconnect() {
    // send the bye message
    sendByeMessage();

    // close the socket
    destruct();
    
    // exit the program
    exit(1);
};

// send the bye message
void ChatUDP::sendByeMessage() {
    MessageByeUDP* byeMessage = new MessageByeUDP(msgCount, client.displayName);

    // handle retransmitions
    for (int attempt = 0; attempt < retransmissions; ++attempt) {
        // send the message
        backendSendMessage(byeMessage->getMessage());
        if (waitForConfirmation(byeMessage)) return; 
    }
};

// sends the timeout error message
void ChatUDP::sendTimeoutErrMessage() {
    MessageErrorUDP* errMessage = new MessageErrorUDP(msgCount, client.displayName, "Timeout while waiting for server response");

    for (int attempt = 0; attempt < retransmissions; ++attempt) {
        // send the message
        backendSendMessage(errMessage->getMessage());
        if (waitForConfirmation(errMessage)) return; 
    }
};

void ChatUDP::backendSendMessage(std::string message) {
    ssize_t bytes_sent = sendto(
        sockfd,
        message.c_str(),
        message.size(),
        0,
        (struct sockaddr*)&receiver,
        sizeof(receiver)
    );

    if (bytes_sent < 0) {
        perror("sendto");
        std::cout << "Error: failed to send a udp message\n";
        exit(1);
    }
}

// method for receiving server response (UDP)
std::string ChatUDP::backendGetServerResponse() {
    socklen_t sender_len = sizeof(sender_addr);

    // Clear the buffer first
    memset(this->buffer, 0, BUFFER_SIZE);

    int bytes_received = recvfrom(
        sockfd,
        this->buffer,
        BUFFER_SIZE - 1,
        0,
        (struct sockaddr*)&sender_addr,
        &sender_len
    );

    if (bytes_received < 0) {
        perror("recvfrom");
        std::cout << "ERROR: receiving UDP message\n";
        handleDisconnect();
        return "";
    }
    
    return std::string(this->buffer, bytes_received);
}

