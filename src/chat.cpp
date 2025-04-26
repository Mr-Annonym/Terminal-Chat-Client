/**
 * @file chat.cpp
 * @brief Implementation of the Chat class
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
#include <chrono>
#include "chat.hpp"

// global wariable, this is only used, since I did not find a better way to pass in the ctrl+c signal to poll
int sigfds[2]; 

// function to handle the ctrl+c signal
void signalHandler(int sig) {
    // Notify poll() about the signalds
    if (write(sigfds[1], &sig, sizeof(sig)) == -1) {
        std::cerr << "ERROR: failed to write to signal socket\n" << std::flush;
        exit(1);
    }; 
}

// Constructor for Chat class
Chat::Chat(NetworkAdress& receiver) {
    cmdFactory = new Command();
    setupAdress(receiver, this->receiver);
    //setNonBlocking(STDIN_FILENO);
    buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
}

// Method to setup the adress struct
void Chat::setupAdress(NetworkAdress& sender, sockaddr_in& addr) {

    addr.sin_family = AF_INET;
    addr.sin_port = htons(sender.port);
    if (inet_pton(AF_INET, sender.ip.c_str(), &addr.sin_addr) <= 0) {
        std::cout << "ERROR: Invalid address/ Address not supported\n" << std::flush;
        exit(1);
    }
}

// Method to set a fd to non-blocking mode
int Chat::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Method to print a status message
void Chat::printStatusMessage(Message* msg) {
    if (msg == nullptr) return; // something went wrong

    bool isOk;
    std::string content;

    if (msg->getType() != MessageType::pREPLY && msg->getType() != MessageType::nREPLY) {
        std::cout << "ERROR: invalid message type, expected REPLY but got MESSAGE\n" << std::flush;
        handleDisconnect(new MessageError(msgCount, client.displayName, "invalid message type"));
    }
    
    // dynamic cast only used for casting inharited classes
    MessageReply* msgReply = dynamic_cast<MessageReply*>(msg);

    isOk = msgReply->isReplyOk();
    content = msgReply->getContent();

    // print the status message
    std::string status = isOk ? "Success" : "Failure";  
    std::cout << "Action " << status << ": " << content << std::endl << std::flush;
}

// Method to print out a message form the server
void Chat::printMessage(Message* msg) {
    if (msg == nullptr) return; // something went wrong

    if (msg->getType() != MessageType::MSG) {
        std::cout << "ERROR: invalid message type, expected MESSAGE but got REPLY\n" << std::flush;
        handleDisconnect(new MessageError(msgCount, client.displayName, "invalid message type"));
    }

    MessageMsg* msgMsg = dynamic_cast<MessageMsg*>(msg);
    std::cout << msgMsg->getDisplayName() << ": " << msgMsg->getContent() << std::endl << std::flush;
}

// Method to check, if a given sent message is valid for the current state
bool Chat::msgTypeValidForStateSent(MessageType type) {

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

// method to validate if a given incoming message is valid for the current state
bool Chat::msgTypeValidForStateReceived(MessageType type) {
    // in case i get err or bye, just exit
    if (type == MessageType::BYE) handleDisconnect(nullptr);

    switch (state) {
        case FSMState::START:
            return false;
        case FSMState::AUTH:
            if (type == MessageType::pREPLY) { // auth ok
                state = FSMState::OPEN;
                return true;
            }
            if (type == MessageType::nREPLY) {  // auth failed
                state = FSMState::START;
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

// Method to handle the user input (convert user input into a command)
Command* Chat::handleUserInput(std::string userInput) {
    if (userInput.empty()) return nullptr;

    // if it is not a rename, return the command
    Command* command = cmdFactory->createCommand(userInput);
    if (command == nullptr) return nullptr;
    if (typeid(*command) != typeid(CommandRename)) return command;

    // handle rename cmd
    CommandRename* renameCmd = dynamic_cast<CommandRename*>(command);
    if (renameCmd) {
        client.displayName = renameCmd->getNewName();
        std::cout << "Action Sucsess: Display name changed to: " << client.displayName << std::endl << std::flush;
    }
    return nullptr;
}

// Method to wait for a response from the server with a timeout
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
    std::cout << "ERROR: internal error, poll failed\n" << std::flush;
    return ""; // To make compiler happy
}

// Method to create the event loop (run the chat client)
void Chat::eventLoop() {
    if (sockfd < 0) {
        std::cout << "ERROR: socket inicialization faild\n" << std::flush;
        exit(1);
    }

    // Create a socket pair for signal handling 
    // credit -> chat GPT
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sigfds) == -1) {
        std::cout << "ERROR: failed to create socket pair\n" << std::flush;
        exit(1);
    }

    // Setup SIGINT handler
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        std::cout << "ERROR: faild to setup sigint\n" << std::flush;
        exit(1);
    }

    struct pollfd fds[3];
    // user input
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // server response
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLHUP; // POLLHUP needs to bere here, in case the stdin would be closed

    // ctrl+c signal
    fds[2].fd = sigfds[0]; // Signal notification via socketpair
    fds[2].events = POLLIN;
    int ret;

    while (true) {
        do {
            ret = poll(fds, 3, -1);
        } while (ret == -1 && errno == EINTR); // Retry on signal interruption (ctr+c someties results in EINTR in my testing)

        // poll error 
        if (ret == -1 && errno != EINTR) {
            std::cout << "ERROR: poll failed\n" << std::flush;
            handleDisconnect(new MessageError(msgCount, client.displayName, "internal client error")); 
        }

        // Check for user input
        if (fds[0].revents & POLLIN || fds[0].revents & POLLHUP) {
            std::string userMessage;
            if (!std::getline(std::cin, userMessage)) { // Handle EOF
                handleDisconnect(new MessageBye(msgCount, client.displayName)); 
            }
            // handle the user input
            if (!userMessage.empty()) sendMessage(userMessage);
        }

        // Check for server response
        if (fds[1].revents & POLLIN) readMessageFromServer();
        
        // Check for SIGINT (Ctrl+C)
        if (fds[2].revents & POLLIN) {
            int sig;
            if (read(sigfds[0], &sig, sizeof(sig)) == -1) {
                std::cerr << "ERROR: failed to read from signal socket\n" << std::flush;
                exit(1);
            };
            handleDisconnect(new MessageBye(msgCount, client.displayName)); 
        }
    }

    // Clean up before exiting (not really needed, but just to be safe)
    close(sigfds[0]);
    close(sigfds[1]);
    destruct();
}
