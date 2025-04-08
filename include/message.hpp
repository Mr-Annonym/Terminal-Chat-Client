
#ifndef MESSAGE_HPP
#define MESSAGE_HPP
#include <string>
#include "command.hpp"
#include "utils.hpp"

struct clientInfo {
    std::string displayName;
    std::string secret;
    std::string username;
    std::string currentChannel;
};

bool startsWith(const std::string& str, const std::string& prefix);

// interface/factory for message
class Message {
    public:
        Message() {}; // default constructor
        Message(clientInfo* client) {this->client = client;}; // constructor with client info (for factory)
        ~Message() {};
        // factory method, to create message form a server response
        virtual Message* readResponse() {return nullptr;};
        // base method for all messages, for converting to desired send format
        virtual std::string getMessage() {return "";};
        // base method for bye message
        virtual void sendByMessage() {};
        // base method for error message
        virtual void sendErrorMessage() {};
        // base method for converting a cmd to a message
        virtual Message* convertCommandToMessage() {return nullptr;};
        virtual MessageType getType() = 0;
        virtual uint16_t getId() = 0;
    protected:
        struct clientInfo* client;
};

// TCP factory 
class TCPMessages : public Message {
    public:
        TCPMessages(clientInfo* client) : Message(client) {};
        Message* readResponse() override {return nullptr;};
        Message* readResponse(std::string resopnse);
        Message* convertCommandToMessage() override {return nullptr;};
        Message* convertCommandToMessage(Command* command);
        MessageType getType() override {return MessageType::UNKNOWN;};
        uint16_t getId() override {return 0;};
};

// UDP factory
class UDPMessages : public Message {
    public:
        UDPMessages(clientInfo* client) : Message(client) {};
        Message* readResponse() override {return nullptr;};
        Message* readResponse(std::string resopnse);
        Message* convertCommandToMessage() override {return nullptr;};
        Message* convertCommandToMessage(uint16_t messageID, Command* command);
        MessageType getType() override {return MessageType::UNKNOWN;}
        // static method for udp ping message
        static unsigned int getNextZeroIdx(std::string message, unsigned int startIdx);
        uint16_t getId() override {return 0;};
}; 

// message Error
class MessageError : public Message {
    public:
        MessageError(std::string displayName, std::string content);
        ~MessageError() {};
        MessageType getType() override {return MessageType::ERR;};
        uint16_t getId() override {return 0;};
        
        protected:
        std::string displayName;
        std::string content;
};

// message Error for UDP
class MessageErrorUDP : public MessageError {
    public:
        MessageErrorUDP(uint16_t msgID, std::string displayName, std::string content);
        ~MessageErrorUDP() {};
        uint16_t getId() override {return msgID;};

        std::string getMessage() override;
    private:
        uint16_t msgID;
};

// message Error for TCP
class MessageErrorTCP : public MessageError {
    public:
        MessageErrorTCP(std::string displayName, std::string content) : MessageError(displayName, content) {};
        ~MessageErrorTCP() {};
        std::string getMessage() override;
        uint16_t getId() override {return 0;};
};

// message Reply
class MessageReply : public Message {
    public:
        MessageReply(bool isOk, std::string content);
        ~MessageReply() {};
        MessageType getType() override {return isOk ? MessageType::pREPLY : MessageType::nREPLY;};
        bool isReplyOk() const { return isOk; };
        std::string getContent() const { return content; };
        uint16_t getId() override {return 0;};

    protected:
        bool isOk;
        std::string content;
};

// message Reply for UDP
class MessageReplyUDP : public MessageReply {
    public:
        MessageReplyUDP(uint16_t msgID, bool isOk, uint16_t refMsgID, std::string content);
        ~MessageReplyUDP() {};
        std::string getMessage() override;
        uint16_t getId() override {return msgID;};

    private:
        uint16_t msgID;
        uint16_t refMsgID;
};

// message Reply for TCP
class MessageReplyTCP : public MessageReply {
    public:
        MessageReplyTCP(bool isOk, std::string content) : MessageReply(isOk, content) {};
        ~MessageReplyTCP() {};
        std::string getMessage() override;
        uint16_t getId() override {return 0;};
};

// message Auth
class MessageAuth : public Message {
    public:
        MessageAuth(std::string username, std::string displayName, std::string secret);
        ~MessageAuth() {};
        MessageType getType() override {return MessageType::AUTH;};
        uint16_t getId() override {return 0;};

    protected:
        std::string username;
        std::string displayName;
        std::string secret;
};

// message Auth for UDP
class MessageAuthUDP : public MessageAuth {
    public:
        MessageAuthUDP(uint16_t msgID, std::string username, std::string displayName, std::string secret);
        ~MessageAuthUDP() {};
        std::string getMessage() override;
        uint16_t getId() override {return msgID;};

    private:
        uint16_t msgID;
};

// message Auth for TCP
class MessageAuthTCP : public MessageAuth {
    public:
        MessageAuthTCP(std::string username, std::string displayName, std::string secret) : MessageAuth(username, displayName, secret) {};
        ~MessageAuthTCP() {};
        std::string getMessage() override;
        uint16_t getId() override {return 0;};
};

// message Join
class MessageJoin : public Message {
    public:
        MessageJoin(std::string channelId, std::string displayName);
        ~MessageJoin() {};
        MessageType getType() override {return MessageType::JOIN;};
        uint16_t getId() override {return 0;};

    protected:
        std::string displayName;
        std::string channelId;
};

// message Join for UDP
class MessageJoinUDP : public MessageJoin {
    public:
        MessageJoinUDP(uint16_t msgID, std::string channelId, std::string displayName);
        ~MessageJoinUDP() {};
        std::string getMessage() override;
        uint16_t getId() override {return msgID;};

    private:
        uint16_t msgID;
};

// message Join for TCP
class MessageJoinTCP : public MessageJoin {
    public:
        MessageJoinTCP(std::string channelId, std::string displayName) : MessageJoin(channelId, displayName) {};
        ~MessageJoinTCP() {};
        std::string getMessage() override;
        uint16_t getId() override {return 0;};
};

// message Msg
class MessageMsg : public Message {
    public:
        MessageMsg(std::string displayName, std::string content);
        ~MessageMsg() {};
        MessageType getType() override {return MessageType::MSG;};
        std::string getDisplayName() const { return displayName; };
        std::string getContent() const { return content; };
        uint16_t getId() override {return 0;};

    protected:
        std::string displayName;
        std::string content;
};

// message Msg for UDP
class MessageMsgUDP : public MessageMsg {
    public:
        MessageMsgUDP(uint16_t msgID, std::string displayName, std::string content);
        ~MessageMsgUDP() {};
        std::string getMessage() override;
        uint16_t getId() override {return msgID;};

    private:
        uint16_t msgID;
};

// message Msg for TCP
class MessageMsgTCP : public MessageMsg {
    public:
        MessageMsgTCP(std::string displayName, std::string content) : MessageMsg(displayName, content) {};
        ~MessageMsgTCP() {};
        std::string getMessage() override;
        uint16_t getId() override {return 0;};
};

// message bye
class MessageBye : public Message {
    public:
        MessageBye(std::string displayName);
        ~MessageBye() {};
        MessageType getType() override {return MessageType::BYE;};
        uint16_t getId() override {return 0;};

    protected:
        std::string displayName;
};

// message bye for UDP
class MessageByeUDP : public MessageBye {
    public:
        MessageByeUDP(uint16_t msgID, std::string displayName);
        ~MessageByeUDP() {};
        std::string getMessage() override;
        uint16_t getId() override {return msgID;};

    private:
        uint16_t msgID;
};

// message bye for TCP
class MessageByeTCP : public MessageBye {
    public:
        MessageByeTCP(std::string displayName) : MessageBye(displayName) {};
        ~MessageByeTCP() {};
        std::string getMessage() override;
        uint16_t getId() override {return 0;};
};

// CONFIRM message
class MessageConfirm : public Message {
    public:
        MessageConfirm(uint16_t msgID) {ID= msgID;};
        ~MessageConfirm() {};
        std::string getMessage() override;
        uint16_t getId() override { return ID; };
        MessageType getType() override {return MessageType::CONFIRM;};

    private:
        uint16_t ID;
};

// PING message
class MessagePing : public Message {
    public:
        MessagePing(uint16_t msgID) {ID= msgID;};
        ~MessagePing() {};
        std::string getMessage() override;
        uint16_t getId() override{ return ID; };
        MessageType getType() override {return MessageType::PING;};

    private:
        uint16_t ID;
};

#endif // MESSAGE_HPP