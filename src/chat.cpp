#include <iostream>
#include <string>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <csignal>
#include <sys/socket.h>
#include <chrono>
#include "chat.hpp"

int sigfds[2]; 

void signalHandler(int sig) {
    // Notify poll() about the signalds
    write(sigfds[1], &sig, sizeof(sig)); 
}

Chat::Chat(NetworkAdress& receiver) {
    cmdFactory = new Command();
    setupAdress(receiver, this->receiver);
    //setNonBlocking(STDIN_FILENO);
    buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
}

void Chat::setupAdress(NetworkAdress& sender, sockaddr_in& addr) {

    addr.sin_family = AF_INET;
    addr.sin_port = htons(sender.port);
    if (inet_pton(AF_INET, sender.ip.c_str(), &addr.sin_addr) <= 0) {
        std::cout << "ERROR: Invalid address/ Address not supported\n";
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
    std::string content;

    if (dynamic_cast<MessageReplyTCP*>(msg)) {
        MessageReplyTCP* msgMsg = dynamic_cast<MessageReplyTCP*>(msg);
        isOk = msgMsg->isReplyOk();
        content = msgMsg->getContent();
    }
    if (dynamic_cast<MessageReplyUDP*>(msg)) {
        MessageReplyUDP* msgMsg = dynamic_cast<MessageReplyUDP*>(msg);
        isOk = msgMsg->isReplyOk();
        content = msgMsg->getContent();
    }
    std::string status = isOk ? "Success" : "Failed";  
    std::cout << "Action " << status << ": " << content << std::endl;
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
            if (type == MessageType::AUTH) {
                state = FSMState::AUTH;
                return true;
            }
            return false;
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
    return false;
}

bool Chat::msgTypeValidForStateReceived(MessageType type) {
    // in case i get err or bye, just exit
    if (
        type == MessageType::ERR || 
        type == MessageType::BYE || 
        type == MessageType::CONFIRM
    ) handleDisconnect();

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
    return false;
}

Command* Chat::handleUserInput(std::string userInput) {
    if (userInput.empty()) return nullptr;

    Command* command = cmdFactory->createCommand(userInput);
    if (typeid(*command) != typeid(CommandRename)) return command;

    CommandRename* renameCmd = dynamic_cast<CommandRename*>(command);
    if (renameCmd) {
        client.displayName = renameCmd->getNewName();
        std::cout << "Action sucsess: Display name changed to: " << client.displayName << std::endl;
    }
    return nullptr;
}

std::string Chat::waitForResponse(int* timeLeft) {

    if (!timeLeft || *timeLeft <= 0) return "";

    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN;

    auto start_time = std::chrono::steady_clock::now();

    int ret;
    do {
        ret = poll(&pfd, 1, *timeLeft);
    } while (ret == -1 && errno == EINTR); // Retry on signal interruption

    auto end_time = std::chrono::steady_clock::now();
    int elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Update the time left
    *timeLeft = std::max(0, *timeLeft - elapsed_time);

    // Timeout or error
    if (ret <= 0) return "";

    // Response
    if (pfd.revents & POLLIN) return backendGetServerResponse();

    // Error
    std::cout << "ERROR: internal error, poll failed\n";
    handleDisconnect();
    return ""; // To make compiler happy
}

void Chat::eventLoop() {
    if (sockfd < 0) {
        std::cout << "ERROR: socket inicialization faild\n";
        exit(1);
    }

    // Create a socket pair for signal handling
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sigfds) == -1) {
        std::cout << "ERROR: failed to create socket pair\n";
        exit(1);
    }

    // Setup SIGINT handler
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        std::cout << "ERROR: faild to setup sigint\n";
        exit(1);
    }

    struct pollfd fds[3];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    fds[2].fd = sigfds[0]; // Signal notification via socketpair
    fds[2].events = POLLIN;
    int ret;

    while (true) {
        do {
            ret = poll(fds, 3, -1);
        } while (ret == -1 && errno == EINTR); // Retry on signal interruption

        // poll error 
        if (ret == -1 && errno != EINTR) {
            std::cout << "ERROR: poll failed\n";
            handleDisconnect(); // Custom method to send disconnect signal
        }

        // Check for user input
        if (fds[0].revents & POLLIN) {
            std::string userMessage;
            if (!std::getline(std::cin, userMessage)) { // Handle EOF
                handleDisconnect(); // Custom method to send disconnect signal
            }
            if (!userMessage.empty()) sendMessage(userMessage);
        }

        // Check for server response
        if (fds[1].revents & POLLIN) readMessageFromServer();
        
        // Check for SIGINT (Ctrl+C)
        if (fds[2].revents & POLLIN) {
            int sig;
            read(sigfds[0], &sig, sizeof(sig));
            handleDisconnect(); // Custom method to send disconnect signal
        }
    }

    // Clean up before exiting
    close(sigfds[0]);
    close(sigfds[1]);
    destruct();
}
