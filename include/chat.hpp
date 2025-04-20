/**
 * @file chat.hpp
 * @brief Header file for the Chat class and its derived classes
 * @author Martin Mendl <x247581>
 * @date 18.4.2025
 */

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

/**
 * @class Chat
 * @brief Abstract base class providing the common framework for TCP and UDP chat implementations.
 *
 * This class defines a common interface for TCP and UDP chat clients.
 * It manages the finite state machine (FSM) logic, user input handling,
 * and polling for I/O. Subclasses implement protocol-specific behavior.
 */
class Chat {
    public:
        /**
         * @brief Constructs a Chat object with a specified receiver address.
         * @param receiver The target network address to communicate with.
         */
        Chat(NetworkAdress& receiver);
    
        /// Virtual destructor.
        virtual ~Chat() {};
    
        /**
         * @brief Starts the main polling loop to send and receive messages.
         * 
         * This function initializes epoll, handles user input and incoming messages,
         * and transitions between FSM states accordingly.
         */
        void eventLoop();
    
    protected:
        /// Sends a user message (implemented in derived class).
        virtual void sendMessage(std::string userInput) = 0;
    
        /// Handles disconnection logic (implemented in derived class).
        virtual void handleDisconnect(Message* exitMsg) = 0;
    
        /// Parses a raw response from the server into a Message (implemented in derived class).
        virtual Message* parseResponse(std::string response) = 0;
    
        /// Cleans up internal resources before program exit (implemented in derived class).
        virtual void destruct() = 0;
    
        /// Receives and processes incoming messages from the server.
        virtual void readMessageFromServer() = 0;
    
        /// Handles a received message and performs the appropriate action.
        virtual void handleIncommingMessage(Message* message) = 0;
    
        /// Fetches a raw response string from the backend (network socket).
        virtual std::string backendGetServerResponse() = 0;
    
        /// Sends a raw message string using the backend socket.
        virtual void backendSendMessage(std::string message) = 0;
    
        /// Parses user input and returns a Command object.
        Command* handleUserInput(std::string userInput);
    
        /// Validates whether a message type is allowed to be sent in the current FSM state.
        bool msgTypeValidForStateSent(MessageType type);
    
        /// Validates whether a message type is allowed to be received in the current FSM state.
        bool msgTypeValidForStateReceived(MessageType type);
    
        /// Helper to set up a socket address from a NetworkAdress object.
        void setupAdress(NetworkAdress& sender, sockaddr_in& addr);
    
        /// Prints a status message (e.g., JOIN/LEAVE notifications).
        void printStatusMessage(Message* msg);
    
        /// Prints a user-to-user message.
        void printMessage(Message* msg);
    
        /// Waits for a server response and returns it, handles timeout via pointer.
        std::string waitForResponse(int* timeLeft);
    
        /// Makes the given file descriptor non-blocking.
        int setNonBlocking(int fd);
    
        /// Frees the allocated buffer.
        void deleteBuffer() {
            if (buffer != nullptr) {
                free(buffer);
                buffer = nullptr;
            }
        };
    
        Command* cmdFactory;              ///< Factory for generating commands.
        int sockfd = -1;                  ///< Socket file descriptor.
        sockaddr_in receiver;             ///< Receiver address (target).
        sockaddr_in sender_addr;          ///< Local sender address.
        struct clientInfo client;         ///< Client metadata and state.
        int epoll_fd;                     ///< Epoll file descriptor for polling events.
        std::vector<Message*> backlog;    ///< Unprocessed incoming messages.
        FSMState state = FSMState::START; ///< Current FSM state.
        char* buffer;                     ///< Temporary buffer for message I/O.
        int timeout_ms = 5000;            ///< Default timeout in milliseconds.
        uint16_t msgCount = 0;            ///< Number of messages sent.

};
    

/**
 * @class ChatTCP
 * @brief TCP-specific implementation of the Chat interface.
 *
 * Handles connection-based reliable message sending/receiving using TCP sockets.
 */
class ChatTCP : public Chat {
    public:
        /**
         * @brief Constructs a TCP chat client with the target receiver address.
         * @param receiver Network address of the server to connect to.
         */
        ChatTCP(NetworkAdress& receiver);
    
        /// Destructor to clean up allocated buffers and resources.
        ~ChatTCP();
    
    private:
        void readMessageFromServer() override;
        std::string backendGetServerResponse() override;
        void backendSendMessage(std::string message) override;
        void sendMessage(std::string userInput) override;
        void handleDisconnect(Message* exitMsg) override;
        void destruct() override;
        void handleIncommingMessage(Message* message) override;
        Message* parseResponse(std::string response) override;
    
        /**
         * @brief Waits for a server response with a defined timeout 
         */
        void waitForResponseWithTimeout();

        TCPMessages* tcpFactory;                        ///< Factory for TCP protocol message creation.
        MessageBuffer* msgBuffer = new MessageBuffer(); ///< Buffer to accumulate incomplete TCP messages.
        std::string currentMessage;                     ///< Currently read message from socket.
};
    

/**
 * @class ChatUDP
 * @brief UDP-specific implementation of the Chat interface.
 *
 * Handles unreliable message sending/receiving with retransmissions
 */
class ChatUDP : public Chat {
    public:
        /**
         * @brief Constructs a UDP chat client with retransmission configuration.
         * @param sender Local sender address.
         * @param retransmissions Number of retransmission attempts.
         * @param timeout Timeout duration for acknowledgements in milliseconds.
         */
        ChatUDP(NetworkAdress& sender, int retransmissions, int timeout);
    
        /// Destructor to release resources and buffers.
        ~ChatUDP();
    
    private:
        void readMessageFromServer() override;
        std::string backendGetServerResponse() override;
    
        void backendSendMessage(std::string message) override;
        void sendMessage(std::string userInput) override;
        void handleDisconnect(Message* exitMsg) override;
        void destruct() override; 
        void handleIncommingMessage(Message* message) override;
        Message* parseResponse(std::string response) override;

        /**
         * @brief Method that handles confirmation of messages.
         * @param message The incoming message string.
         * @return True if the message needs further processing, false if it is a confirmation/ping.
         */
        bool handleConfirmation(std::string message);
    
        
        /**
         * @brief Sends a message with retransmission.
         * @param msg Pointer to the message to send.
         */
        void transmitMessage(Message* msg);
    
        /**
         * @brief Waits for a confirmation message to arrive.
         * @param message The original sent message.
         * @return True if confirmation received, false if timeout.
         */
        bool waitForConfirmation(Message* message);
    
        /**
         * @brief Waits for a response and confirms it matches the given message.
         * @param msg The original message expecting confirmation.
         */
        void waitForResponseWithTimeout(Message* msg);
        
        uint16_t lastShownServerMsgID = 0;       ///< Last received and shown message ID from server.
        bool confirmedAtLeastOneMessage = false; ///< Flag to check if at least one message was confirmed.
        UDPMessages* udpFactory;                 ///< UDP message factory for message creation.
        int retransmissions = 0;                 ///< Max number of retransmissions per message.
        int timeout = 0;                         ///< Timeout for waiting for confirmation.
};
    

#endif // CHAT_HPP