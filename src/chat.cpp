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

int sigfds[2]; 

void signalHandler(int sig) {
    // Notify poll() about the signalds
    write(sigfds[1], &sig, sizeof(sig)); 
}

Chat::Chat(NetworkAdress& receiver) {
    cmdFactory = new Command();
    setupAdress(receiver, this->receiver);
    setNonBlocking(STDIN_FILENO);
    buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
}

void Chat::setupAdress(NetworkAdress& sender, sockaddr_in& addr) {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(sender.port);
    if (inet_pton(AF_INET, sender.ip.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported\n";
        exit(1);
    }
}

int Chat::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void Chat::printStatusMessage(Message* msg) {
    if (msg == nullptr) return;
    std::string actionName = (state == FSMState::AUTH) ? "Auth" : "Join";
    bool isOk;

    if (dynamic_cast<MessageReplyTCP*>(msg)) {
        MessageReplyTCP* msgMsg = dynamic_cast<MessageReplyTCP*>(msg);
        isOk = msgMsg->isReplyOk();
    }
    if (dynamic_cast<MessageReplyUDP*>(msg)) {
        MessageReplyUDP* msgMsg = dynamic_cast<MessageReplyUDP*>(msg);
        isOk = msgMsg->isReplyOk();
    }
    std::string status = isOk ? "success" : "failed";
    std::string sutff = isOk ? "Success" : "Failed";  

    std::cout << "Action " << sutff << ": " << actionName << " " << status << "." << std::endl;
}

void Chat::printMessage(Message* msg) {
    if (msg == nullptr) return;
    if (dynamic_cast<MessageMsgTCP*>(msg)) {
        MessageMsgTCP* msgMsg = dynamic_cast<MessageMsgTCP*>(msg);
        std::cout << msgMsg->getDisplayName() << ": " << msgMsg->getContent() << std::endl;
        return;
    }
    if (dynamic_cast<MessageMsgUDP*>(msg)) {
        MessageMsgUDP* msgMsg = dynamic_cast<MessageMsgUDP*>(msg);
        std::cout << msgMsg->getDisplayName() << ": "<< msgMsg->getContent() << std::endl;
    }
}

bool Chat::msgTypeValidForStateSent(MessageType type) {

    // bye can be sent form anywhere, exit program
    if (type == MessageType::BYE) handleDisconnect();

    switch (state) {
        case FSMState::START:
            switch(type) {
                case MessageType::AUTH:
                    state = FSMState::AUTH;
                    return true;
                default:
                    return false;
            }
        case FSMState::AUTH:
            switch(type) {
                case MessageType::AUTH:
                case MessageType::ERR:
                    return true;
                default:
                    return false;
            }
        case FSMState::OPEN:
            switch(type) {
                case MessageType::MSG:
                case MessageType::ERR:
                    return true;
                case MessageType::JOIN:
                    state = FSMState::JOIN;
                    return true;
                default:
                    return false;
            }
        case FSMState::JOIN:
        case FSMState::END:
            return false;
    }
}

bool Chat::msgTypeValidForStateReceived(MessageType type) {
    // in case i get err or bye, just exit
    if (type == MessageType::ERR || type == MessageType::BYE) handleDisconnect();

    switch (state) {
        case FSMState::START:
            return false;
        case FSMState::AUTH:
            if (type == MessageType::pREPLY) { // auth ok
                state = FSMState::OPEN;
                return true;
            }
            if (type == MessageType::nREPLY) {  // auth failed
                return true;
            }
            return false;
        case FSMState::OPEN:
            switch(type) {
                case MessageType::MSG:
                    return true;
                default:
                    return false;
            }
        case FSMState::JOIN:
            switch(type) {
                case MessageType::MSG:
                    return true;
                case MessageType::pREPLY:
                case MessageType::nREPLY:
                    state = FSMState::OPEN;
                    return true;
                default:
                    return false;
            }
        case FSMState::END:
            return false;
    }
}

Command* Chat::handleUserInput(std::string userInput) {
    if (userInput.empty()) return nullptr;

    Command* command = cmdFactory->createCommand(userInput);
    return command;
}

void Chat::handleIncommingMessage(Message* message) {
    if (message == nullptr) handleDisconnect();

    // check, if the response is allowed
    if (!msgTypeValidForStateReceived(message->getType())) {
        std::string errMsg;
        if (state == FSMState::AUTH) {
            errMsg = "Invalid message, expected REPLY but go MESSAGE";
        } else if (state == FSMState::OPEN) {
            errMsg = "Invalid message, expected MESSAGE but got REPLY";
        }
        MessageError* errorMessage = new MessageError(client.displayName, errMsg);
        // DELETE
        backendSendMessage(errorMessage->getMessage());
        handleDisconnect();
    } 
    
    // user Message
    if (typeid(*message) == typeid(MessageMsgTCP) || typeid(*message) == typeid(MessageMsgUDP)) {
        printMessage(message);
        return;
    }

    // Repaly
    printStatusMessage(message);
}

void Chat::eventLoop() {
    if (sockfd1 < 0) {
        std::cerr << "Socket is not initialized\n";
        return;
    }

    // Create a socket pair for signal handling
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sigfds) == -1) {
        std::cerr << "Failed to create socket pair\n";
        exit(1);
    }

    // Setup SIGINT handler
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        std::cerr << "Failed to set signal handler\n";
        exit(1);
    }

    struct pollfd fds[3];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd1;
    fds[1].events = POLLIN;

    fds[2].fd = sigfds[0]; // Signal notification via socketpair
    fds[2].events = POLLIN;

    while (true) {
        int ret;
        do {
            ret = poll(fds, 3, -1);
        } while (ret == -1 && errno == EINTR); // Restart poll if interrupted

        // Check for user input
        if (fds[0].revents & POLLIN) {
            std::string userMessage;
            std::getline(std::cin, userMessage);
            if (!userMessage.empty()) {
                sendMessage(userMessage);
            }
        }

        // Check for server response
        if (fds[1].revents & POLLIN) {
            std::string response = getServerResponse();
            Message* message = parseResponse(response);
            handleIncommingMessage(message);
        }

        // Check for SIGINT (Ctrl+C)
        if (fds[2].revents & POLLIN) {
            int sig;
            read(sigfds[0], &sig, sizeof(sig));
            handleDisconnect(); // Custom method to send disconnect signal
            break;
        }
    }

    // Clean up before exiting
    close(sigfds[0]);
    close(sigfds[1]);
    destruct();
}
