

#ifndef CHAT_HPP
#define CHAT_HPP

#include <string>
#include <vector>
#include <netinet/in.h>
#include "command.hpp"
#include "message.hpp"
#include "settings.hpp"
#include "utils.hpp"


class Chat {
    public:
        Chat(NetworkAdress& receiver);
        ~Chat() {deleteBuffer();};
        Command* handleUserInput(std::string userInput);
        virtual void sendMessage(std::string userInput) = 0;
        virtual void handleDisconnect() = 0;
        void eventLoop();
    protected:
        virtual std::string getServerResponse() {return "";};
        virtual Message* parseResponse(std::string response) = 0;
        virtual void destruct() = 0;
        virtual void backendSendMessage(std::string message) = 0;
        virtual void sendByeMessage() = 0;
        bool msgTypeValidForStateSent(MessageType type);
        bool msgTypeValidForStateReceived(MessageType type);
        void setupAdress(NetworkAdress& sender, sockaddr_in& addr);
        void handleIncommingMessage(Message* message);
        void printStatusMessage(Message* msg);
        void printMessage(Message* msg);
        void deleteBuffer() {
            if (buffer != nullptr) {
                free(buffer);
                buffer = nullptr;
            }
        };
        Command* cmdFactory;
        int sockfd1 = -1;
        sockaddr_in receiver;
        struct clientInfo client;   
        int setNonBlocking(int fd);
        int epoll_fd;
        std::vector<Message*> backlog;
        FSMState state = FSMState::START;
        char* buffer;
        int timeout_ms = 10000; // 5 seconds
};

class ChatTCP : public Chat {
    public:
        ChatTCP(NetworkAdress& receiver);
        ~ChatTCP();
        void sendMessage(std::string userInput) override;
        void handleDisconnect() override;
        void destruct() override;
        void sendByeMessage() override;
    private:
        TCPMessages* tcpFactory;
        void backendSendMessage(std::string message) override;
        std::string getServerResponse() override;   
        Message* parseResponse(std::string response) override;
        void waitForResponseWithTimeout();
};

class ChatUDP : public Chat {
    public:
        ChatUDP(NetworkAdress& sender, NetworkAdress& receiver);
        ~ChatUDP();
        void sendMessage(std::string userInput) override;
        void sendByeMessage() override;
        void handleDisconnect() override;
        void destruct() override; 
    private:
        uint16_t msgCount = 0;
        std::vector<UDPmessaStatus> msgStatus;
        UDPMessages* udpFactory;
        void backendSendMessage(std::string message) override;
        Message* parseResponse(std::string response) override;
        int sockfd2;
};


#endif // CHAT_HPP