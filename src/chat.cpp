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
    // Notify poll() about the signal
    write(sigfds[1], &sig, sizeof(sig)); 
}

Chat::Chat(NetworkAdress& receiver) {
    cmdFactory = new Command();
    setupAdress(receiver, this->receiver);
    setNonBlocking(STDIN_FILENO);
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

Command* Chat::handleUserInput(std::string userInput) {
    if (userInput.empty()) return nullptr;

    Command* command = cmdFactory->createCommand(userInput);
    if (command == nullptr) {
        std::cerr << "Invalid command | empty message\n";
        return nullptr;
    }

    if (authenticated && !joined) {
        if (typeid(*command) != typeid(CommandJoin)) {
            std::cerr << "You must join a channel first\n";
            return nullptr;
        }
        joined = true;
    }

    if (!authenticated) {
        if (typeid(*command) != typeid(CommandAuth)) {
            std::cerr << "You must authenticate first\n";
            return nullptr;
        }
        authenticated = true;
    }
    return command;
}

void Chat::handleNewMessage(Message* message) {
    if (message == nullptr) return;
    std::cout << message->getMessage() << std::endl;
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
                std::cout << "sending message: " << userMessage << std::endl;
                sendMessage(userMessage);
            }
        }

        // Check for server response
        if (fds[1].revents & POLLIN) {
            std::string response = getServerResponse();
            std::cout << "Received response: " << response << std::endl;
            if (response.empty()) {
                std::cerr << "Server closed the connection\n";
                break;
            }
            Message* message = parseResponse(response);
            handleNewMessage(message);
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


ChatTCP::ChatTCP(NetworkAdress& receiver) : Chat(receiver) {
    tcpFactory = new TCPMessages(&client);

    sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd1 < 0) {
        std::cerr << "Error opening socket\n";
        destruct();
        exit(1);
    }

    if (connect(sockfd1, (struct sockaddr *)&receiver, sizeof(receiver)) < 0) {
        std::cerr << "Connection failed\n";
        //destruct();
        //exit(1);
    }

    setNonBlocking(sockfd1);
}

void ChatTCP::destruct() {
    if (sockfd1 >= 0) {
        close(sockfd1);
        sockfd1 = -1;
    }
}

ChatTCP::~ChatTCP() {
    destruct();
}

void ChatTCP::sendMessage(std::string userInput) {
    Command* command = handleUserInput(userInput);
    if (command == nullptr) return;

    Message* message = tcpFactory->convertCommandToMessage(command);
    if (message == nullptr) {
        std::cerr << "Invalid command\n";
        return;
    }

    std::string msg = message->getMessage();
    backendSendMessage(msg);
}

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

std::string ChatTCP::getServerResponse() {
    char buffer[1024] = {0};
    int bytes_received = recv(sockfd1, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        std::cerr << "Error receiving message\n";
        return "";
    }
    if (bytes_received == 0) {
        return "";  // Handle disconnection properly in eventLoop()
    }
    return std::string(buffer, bytes_received);
}

Message* ChatTCP::parseResponse(std::string response) {
    return tcpFactory->readResponse(response);
}

void ChatTCP::handleDisconnect() {
    std::cout << "Disconnecting...\n";
    exit(1); // remove
    sendByeMessage();
    destruct();
}

// TODO
void ChatTCP::sendByeMessage() {

}
