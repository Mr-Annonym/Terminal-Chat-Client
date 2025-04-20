/**
 * @file chatUDP.cpp
 * @brief Implementation of the ChatUDP class
 * @author Martin Mendl <x247581>
 * @date 18.4.2025
*/

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <csignal>
#include <sys/socket.h>
#include <cstring>
#include "chat.hpp"

// Constructor for ChatUDP
ChatUDP::ChatUDP(NetworkAdress& receiver, int retransmissions, int timeout) : Chat(receiver) {

    // attributes
    client.displayName = "unknown"; // default value
    this->retransmissions = retransmissions;
    this->timeout = timeout;

    // create the udp factory
    udpFactory = new UDPMessages(&client);

    // setup socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
    if (sockfd < 0) {
        std::cout << "ERROR: faild to create a socket\n" << std::flush;
        destruct();
        exit(1);
    }

    // Set the socket to non-blocking mode since I am using poll
    setNonBlocking(sockfd);
}

// Destructor for ChatUDP
ChatUDP::~ChatUDP() {
    destruct();
}

// method for ending the communication with the server
void ChatUDP::destruct() {
    deleteBuffer(); // free memory
    if (sockfd >= 0) { // close socket
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
    }
}

// method for reading a message from the server
void ChatUDP::readMessageFromServer() {
    // 1. get the server response
    std::string response = backendGetServerResponse();
    // 2. handle confirmation/ping or exit on bye
    if (!handleConfirmation(response)) return;
    // 3. parse the response
    Message* message = parseResponse(response);
    // 4. handle the message
    handleIncommingMessage(message);
}

// method for parsing the server response
Message* ChatUDP::parseResponse(std::string response) {
    // parse the response and return the message object
    return udpFactory->readResponse(response);
}

// method for sending a message to the server
void ChatUDP::sendMessage(std::string userInput) {

    // parse the user input
    Command* command = handleUserInput(userInput);
    if (command == nullptr) return; // invalid user input

    // create the message, form the user Command
    Message* msg = udpFactory->convertCommandToMessage(msgCount, command);

    // check if the message is good with the FSM
    if (msg == nullptr || !msgTypeValidForStateSent(msg->getType())) {
        if (state == FSMState::START) {
            std::cout << "ERROR: you authenticate first\n" << std::flush;
            return;
        }
        std::cout << "ERROR: invalid input, try again or use /help\n" << std::flush;
        return;
    }

    // send the message
    transmitMessage(msg);
}

// method for sending a message to the server
void ChatUDP::transmitMessage(Message* msg) {

    MessageType type = msg->getType();
    
    // handle retransmitions
    for (int attempt = 0; attempt < retransmissions; ++attempt) {
        // send the message
        backendSendMessage(msg->getUDPMsg());
        
        // wait for a confirmation, oterwise retransmit
        if (waitForConfirmation(msg)) {
            msgCount++;
            // if we are not expecting a reply, we can return
            if (type != MessageType::AUTH && type != MessageType::JOIN) return;
            // expecting a reply msg, wait for it
            waitForResponseWithTimeout(msg);
            return;
        };
    }
    
    // no response -> timeout
    std::cout << "ERROR: connection dropped\n" << std::flush;
    handleDisconnect(nullptr);
};

// method to handle confirming messages (should be run, after i get a response form the server)
bool ChatUDP::handleConfirmation(std::string message) {

    bool ignore = false;

    // if the messsage is not at least 3 bytes long, just return, errors are handled outside
    if (message.length() < 3) return true;
    // firstly, need to extract the first byte, to determin, what messsage it si
    uint8_t msgType = message[0];
    // get the msgId
    uint16_t msgID = static_cast<uint16_t>(message[1]) << 8 | static_cast<uint16_t>(message[2]);

    // in case we got a comfirmation message we just return
    if (msgType == 0x00) return false;

    // update last seen mesasge ID
    if (msgID <= lastShownServerMsgID && confirmedAtLeastOneMessage) {
        ignore = true;
    } else {
        lastShownServerMsgID = msgID;
        confirmedAtLeastOneMessage = true;
    }

    // create a confirm message and send it to the server
    MessageConfirm* msg = new MessageConfirm(msgID);

    // bye message or err msg, this mean we need to wait up to n times for a retransmit, each time send a confirm
    if (msgType == 0xFF || msgType == 0xFE) {

        if (msgType == 0xFF) {
            // got a bye message, need to disconnect
            std::cout << "ERROR: server disconnected\n" << std::flush;
        } else {
            MessageError* errMsg = dynamic_cast<MessageError*>(udpFactory->readResponse(message));
            std::cout << "ERROR FROM " << errMsg->getDisplayName() << ": " << errMsg->getContent() << std::endl << std::flush;
        }

        int timeout;

        for (int attempt = 0; attempt < retransmissions; ++attempt) {
            timeout = this->timeout;
            // send the confirm
            backendSendMessage(msg->getUDPMsg());
            // wait for a response
            std::string serverResponse = waitForResponse(&timeout);
            if (serverResponse.empty()) break; // no new message sent, break the loop
        }
      
        destruct();
        exit(0);
    }
    backendSendMessage(msg->getUDPMsg());

    // if we got a ping message, we dont need futher action
    return msgType == 0xFD ? false : !ignore;
}

// method for waiting for a confirm message
bool ChatUDP::waitForConfirmation(Message* msg) {

    // timeout set during constructor
    int timeLeft = this->timeout;
    uint16_t msgID = msg->getId();

    // if there is time left
    while (timeLeft > 0) {
        // get a response
        std::string responseOut = waitForResponse(&timeLeft);

        // not tome left -> timeout
        if (responseOut.empty() && timeLeft <= 0) {
            return false;
        }

        // parse the message
        Message* message = parseResponse(responseOut);

        // messages other than confirm/ping need to be handled
        if (handleConfirmation(responseOut)) {
            handleIncommingMessage(message);
            continue;
        }

        // ignore ping, it was handled
        if (message->getType() == MessageType::PING) continue;

        // comfirm, but not for this message
        if (msgID != message->getId()) continue;
        // we have a good reply
        return true;
    }
    return false;
}

// method for waiting for a responce from the server (eg. for a reply)
void ChatUDP::waitForResponseWithTimeout(Message* msg) {

    int timeLeft = timeout_ms;
    uint16_t msgID = msg->getId();


    // if there is time left
    while (timeLeft > 0) {

        // read form server
        std::string responseOut = waitForResponse(&timeLeft);

        // ping and confirmation are handled here
        if (!handleConfirmation(responseOut)) continue; 

        // timeout
        if (responseOut.empty() && timeLeft <= 0) {
            std::cout << "ERROR: timeout on message recv\n" << std::flush;
            handleDisconnect(new MessageError(msgCount, client.displayName, "timeout on message recv"));
        }

        // parse response
        Message* message = parseResponse(responseOut);

        // not a valid response
        if (message == nullptr) {
            std::cout << "ERROR: invalid message/malformed message\n" << std::flush;
            handleDisconnect(new MessageError(msgCount, client.displayName, "invalid message/malformed message received"));
        }
        
        MessageType type = message->getType();
        
        // not a reply message, need to handle it
        if (type != MessageType::pREPLY && type != MessageType::nREPLY) {
            handleIncommingMessage(message);
            continue;
        }
            
        MessageReply* rplyMsg = dynamic_cast<MessageReply*>(message);
        // bad reply 
        if (msgID != rplyMsg->getRefMsgID()) continue;
        
        // need to call the msgTypeValid for advancing the FSM
        if (!msgTypeValidForStateReceived(type)) {
            std::cout << "ERROR: invalid message type, expected REPLY but got MESSAGE\n" << std::flush;
            handleDisconnect(new MessageError(msgCount, client.displayName, "invalid message type received for curretn state"));
        }

        // Repaly
        printStatusMessage(message);
        return;
    }

    std::cout << "ERROR: connection to server dropped\n" << std::flush;
    handleDisconnect(nullptr);
};

// method for handling incoming messages
void ChatUDP::handleIncommingMessage(Message* message) {
    if (message == nullptr) {
        std::cout << "ERROR: invalid message/malformed message\n" << std::flush;
        handleDisconnect(new MessageError(msgCount, client.displayName, "invalid message/malformed message"));
    };

    // second, check if the response is allowed
    if (!msgTypeValidForStateReceived(message->getType())) {
        std::string errMsg;
        if (state == FSMState::AUTH) {
            errMsg = "Invalid message, expected REPLY but go MESSAGE";
        } else if (state == FSMState::OPEN) {
            errMsg = "Invalid message, expected MESSAGE but got REPLY";
        }
        std::cout << "ERROR: " << errMsg << std::endl << std::flush;
        handleDisconnect(new MessageError(msgCount, client.displayName, "Message did not conform to the FSM"));
    }

    // user Message
    if (message->getType() == MessageType::MSG) {
        printMessage(message);
        return;
    }
}

// send the bye message and closes stuff
void ChatUDP::handleDisconnect(Message* exitMsg) {

    // need to wait for a possible retransmit ...
    if (exitMsg != nullptr) transmitMessage(exitMsg);

    // close the socket
    destruct();
    
    // exit the program
    exit(1);
};

// simple send message to the server
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
        std::cout << "Error: failed to send a udp message\n" << std::flush;
        exit(1);
    }
}

// method for receiving server response 
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
        std::cout << "ERROR: receiving UDP message\n" << std::flush;
        handleDisconnect(new MessageError(msgCount, client.displayName, "internal client error"));
    }

    // handle dynmic port switch
    receiver.sin_port = sender_addr.sin_port; 
    
    return std::string(this->buffer, bytes_received);
}

