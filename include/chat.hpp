

#ifndef CHAT_HPP
#define CHAT_HPP

#include <string>
#include <vector>
#include <netinet/in.h>
#include "command.hpp"
#include "message.hpp"
#include "settings.hpp"

#define MAX_EVENTS 2 
#define BUFFER_SIZE 1024
// Socket pair for signal handling
void signalHandler(int sig);

enum class UDPmessaStatus {
    SENT,
    CONFIRMED,
};

class Chat {
    static Chat* instance; // static instance ptr, for handling ctr+c
    public:
        Chat(NetworkAdress& receiver);
        ~Chat() {};
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
        void setupAdress(NetworkAdress& sender, sockaddr_in& addr);
        void handleNewMessage(Message* message);
        Command* cmdFactory;
        int sockfd1 = -1;
        bool authenticated = false;
        bool joined = false;
        sockaddr_in receiver;
        struct clientInfo client;   
        int setNonBlocking(int fd);
        void setupEpoll();
        int epoll_fd;
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