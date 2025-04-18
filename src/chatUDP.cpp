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

ChatUDP::ChatUDP(NetworkAdress& receiver, int retransmissions, int timeout) : Chat(receiver) {
    client.displayName = "unknown"; // can be changed later
    this->retransmissions = retransmissions;
    this->timeout = timeout;

    udpFactory = new UDPMessages(&client);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cout << "ERROR: faild to create a socket\n" << std::flush;
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
    if (!handleConfirmation(response)) return;
    Message* message = parseResponse(response);
    handleIncommingMessage(message);
}

Message* ChatUDP::parseResponse(std::string response) {
    // parse the response and return the message object
    return udpFactory->readResponse(response);
}

// gets the command, creates the message obj and calls backendSendMessage
void ChatUDP::sendMessage(std::string userInput) {

    // first send the message
    Command* command = handleUserInput(userInput);
    if (command == nullptr) return;

    Message* msg = udpFactory->convertCommandToMessage(msgCount, command);

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


void ChatUDP::transmitMessage(Message* msg) {
    // handle retransmitions
    MessageType type = msg->getType();
    for (int attempt = 0; attempt < retransmissions; ++attempt) {
        // send the message
        backendSendMessage(msg->getMessage());
        
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
   
    std::cout << "ERROR: connection dropped\n" << std::flush;
    handleDisconnect();
};

bool ChatUDP::handleConfirmation(std::string message) {

    // if the messsage is not at least 3 bytes long, just return, errors are handles outside
    if (message.length() < 3) return true;
    // firstly, need to extrac the first byte, to determin, what messsage it si
    uint8_t msgType = message[0];
    // in case we got a comfirmation message we just return
    if (msgType == 0x00) return false;
    // need to send a confirmation back, that i got the message
    uint16_t msgID = static_cast<uint16_t>(message[1]) << 8 | static_cast<uint16_t>(message[2]);
    // get the msgId

    // create a confirm message and send it to the server
    MessageConfirm* msg = new MessageConfirm(msgID);
    backendSendMessage(msg->getMessage());

    if (msgType == 0xFF) {
        // got a bye message, need to disconnect
        std::cout << "ERROR: server disconnected\n" << std::flush;
        exit(0);
    }
    return (msgType == 0xFD)? false :true;
}

bool ChatUDP::waitForConfirmation(Message* msg) {

    int timeLeft = this->timeout;
    uint16_t msgID = msg->getId();

    while (timeLeft > 0) {
        std::string responseOut = waitForResponse(&timeLeft);

        // timeout
        if (responseOut.empty() && timeLeft <= 0) {
            return false;
        }

        Message* message = parseResponse(responseOut);

        // messages other than confirm/ping
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

// when this receaves a msg, it will send a comfirm back (mby not needed) 
void ChatUDP::waitForResponseWithTimeout(Message* msg) {

    int timeLeft = timeout_ms;
    uint16_t msgID = msg->getId();

    while (timeLeft > 0) {
        std::string responseOut = waitForResponse(&timeLeft);
        // ping and confirmation are handled here
        if (!handleConfirmation(responseOut)) continue; 

        // timeout
        if (responseOut.empty() && timeLeft <= 0) {
            std::cout << "ERROR: timeout on message recv\n" << std::flush;
            sendTimeoutErrMessage();
            handleDisconnect();
        }

        Message* message = parseResponse(responseOut);

        // not a valid response
        if (message == nullptr) {
            std::cout << "ERROR: invalid message/malformed message\n" << std::flush;
            MessageErrorUDP* errMsg = new MessageErrorUDP(msgCount, client.displayName, "Invalid message received");
            transmitMessage(errMsg);
            handleDisconnect();
        }
        
        MessageType type = message->getType();
        
        if (type != MessageType::pREPLY && type != MessageType::nREPLY) handleIncommingMessage(message);

        // reply to msg
        if (msgID != msg->getId()) continue;
        
        // we have a good reply
        if (!msgTypeValidForStateReceived(type)) {
            std::cout << "ERROR: invalid message type, expected REPLY but got MESSAGE\n" << std::flush;
            MessageErrorUDP* errMsg = new MessageErrorUDP(msgCount, client.displayName, "Invalid message type");
            transmitMessage(errMsg);
            handleDisconnect();
        }
        // Repaly
        printStatusMessage(message);
        return;
    }

    std::cout << "ERROR: connection to server dropped\n" << std::flush;
    sendTimeoutErrMessage();
    handleDisconnect();
};

// method for handling incoming messages
void ChatUDP::handleIncommingMessage(Message* message) {
    if (message == nullptr) {
        std::cout << "ERROR: invalid message/malformed message\n" << std::flush;
        MessageErrorUDP* errMsg = new MessageErrorUDP(msgCount, client.displayName, "invalid message/malformed message");
        transmitMessage(errMsg);
        handleDisconnect();
    };

    // we got an error message
    if (message->getType() == MessageType::ERR) {
        MessageErrorUDP* errMsg = dynamic_cast<MessageErrorUDP*>(message);
        if (!errMsg){
            std::cerr << "faild to dynamic cast message to error message\n" << std::flush;
            exit(1);
        }
        std::cout << "ERROR FROM " << errMsg->getDisplayName() << ": " << errMsg->getContent() << std::endl << std::flush;
        handleDisconnect();
        return;
    }

    // second, check if the response is allowed
    if (!msgTypeValidForStateReceived(message->getType())) {
        std::string errMsg;
        if (state == FSMState::AUTH) {
            errMsg = "Invalid message, expected REPLY but go MESSAGE";
        } else if (state == FSMState::OPEN) {
            errMsg = "Invalid message, expected MESSAGE but got REPLY";
        }
        MessageErrorUDP* errorMessage = new MessageErrorUDP(msgCount, client.displayName, "FSM error");
        backendSendMessage(errorMessage->getMessage());
        handleDisconnect();
        return;
    }

    // user Message
    if (message->getType() == MessageType::MSG) {
        if (message->getId() <= lastShownServerMsgID) return;
        lastShownServerMsgID = message->getId();
        printMessage(message);
        return;
    }
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
    std::cout << "ERROR: connection to server dropped\n" << std::flush;
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
        std::cout << "Error: failed to send a udp message\n" << std::flush;
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
        std::cout << "ERROR: receiving UDP message\n" << std::flush;
        handleDisconnect();
        return "";
    }

    // handle dynmic port switch
    receiver.sin_port = sender_addr.sin_port; 
    
    return std::string(this->buffer, bytes_received);
}

