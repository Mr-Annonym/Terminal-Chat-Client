

#ifndef CHAT_HPP
#define CHAT_HPP

#include <string>
#include <vector>
#include <netinet/in.h>
#include "command.hpp"
#include "message.hpp"
#include "settings.hpp"
#include "utils.hpp"
#include "messageBuffer.hpp"

class Chat {
    public:
        Chat(NetworkAdress& receiver);
        ~Chat() {};
        void eventLoop();
    protected:
        virtual void sendMessage(std::string userInput) = 0;
        virtual void handleDisconnect() = 0;
        virtual Message* parseResponse(std::string response) = 0;
        virtual void destruct() = 0;
        virtual void sendByeMessage() = 0;
        virtual void sendTimeoutErrMessage() = 0;
        virtual void readMessageFromServer() = 0;
        virtual void handleIncommingMessage(Message* message) = 0;
        virtual std::string backendGetServerResponse() = 0;
        virtual void backendSendMessage(std::string message) = 0;
        Command* handleUserInput(std::string userInput);
        bool msgTypeValidForStateSent(MessageType type);
        bool msgTypeValidForStateReceived(MessageType type);
        void setupAdress(NetworkAdress& sender, sockaddr_in& addr);
        void printStatusMessage(Message* msg);
        void printMessage(Message* msg);
        std::string waitForResponse(int* timeLeft);
        int setNonBlocking(int fd);
        void deleteBuffer() {
            if (buffer != nullptr) {
                free(buffer);
                buffer = nullptr;
            }
        };
        Command* cmdFactory;
        int sockfd = -1;
        sockaddr_in receiver;
        sockaddr_in sender_addr;
        struct clientInfo client;   
        int epoll_fd;
        std::vector<Message*> backlog;
        FSMState state = FSMState::START;
        char* buffer;
        int timeout_ms = 5000; // 5 seconds
};

class ChatTCP : public Chat {
    public:
        ChatTCP(NetworkAdress& receiver);
        ~ChatTCP();
    private:
        void readMessageFromServer() override;
        std::string backendGetServerResponse() override;
        void backendSendMessage(std::string message) override;
        void sendMessage(std::string userInput) override;
        void handleDisconnect() override;
        void destruct() override;
        void sendByeMessage() override;
        void handleIncommingMessage(Message* message) override;
        Message* parseResponse(std::string response) override;
        void waitForResponseWithTimeout();
        void sendTimeoutErrMessage() override;
        TCPMessages* tcpFactory;
        MessageBuffer* msgBuffer = new MessageBuffer();
        std::string currentMessage;
};

class ChatUDP : public Chat {
    public:
        ChatUDP(NetworkAdress& sender, int retransmissions, int timeout);
        ~ChatUDP();
    private:
        void readMessageFromServer() override;
        std::string backendGetServerResponse() override;
        bool handleConfirmation(std::string message);
        void backendSendMessage(std::string message) override;
        void sendMessage(std::string userInput) override;
        void sendByeMessage() override;
        void handleDisconnect() override;
        void destruct() override; 
        void handleIncommingMessage(Message* message) override;
        Message* parseResponse(std::string response) override;
        void sendTimeoutErrMessage() override;
        void transmitMessage(Message* msg);
        bool waitForConfirmation(Message* message);
        void waitForResponseWithTimeout(Message* msg);
        uint16_t msgCount = 0;
        uint16_t lastShownServerMsgID = 0;
        std::vector<UDPmessaStatus> msgStatus;
        UDPMessages* udpFactory;
        int retransmissions = 0;
        int timeout = 0;
};

#endif // CHAT_HPP