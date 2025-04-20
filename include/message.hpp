
#ifndef MESSAGE_HPP
#define MESSAGE_HPP
#include <string>
#include "command.hpp"
#include "utils.hpp"
#include <cstdint>

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
        Message(uint16_t msgID) {this->msgID = msgID;}; // default constructor
        Message(clientInfo* client) {this->client = client;}; // constructor with client info (for factory)
        ~Message() {};
        // factory method, to create message form a server response
        virtual Message* readResponse() {return nullptr;};
        // base method for converting a cmd to a message
        virtual Message* convertCommandToMessage() {return nullptr;};
        virtual MessageType getType() = 0;
        uint16_t getId() const {return msgID;};
        virtual std::string getTCPMsg() {return "";};
        virtual std::string getUDPMsg() {return "";};
    protected:
        struct clientInfo* client = nullptr;
        uint16_t msgID;
};

// TCP factory 
class TCPMessages : public Message {
    public:
        TCPMessages(clientInfo* client) : Message(client) {};
        Message* readResponse() override {return nullptr;};
        Message* readResponse(const std::string& resopnse);
        Message* convertCommandToMessage() override {return nullptr;};
        Message* convertCommandToMessage(Command* command);
        MessageType getType() override {return MessageType::UNKNOWN;};
};

// UDP factory
class UDPMessages : public Message {
    public:
        UDPMessages(clientInfo* client) : Message(client) {};
        Message* readResponse() override {return nullptr;};
        Message* readResponse(const std::string& resopnse);
        Message* convertCommandToMessage() override {return nullptr;};
        Message* convertCommandToMessage(uint16_t messageID, Command* command);
        MessageType getType() override {return MessageType::UNKNOWN;}
        // static method for udp ping message
        static std::size_t getNextZeroIdx(const std::string& message, std::size_t startIdx);
}; 

// message Error
class MessageError : public Message {
    public:
    MessageError(uint16_t msgID, const std::string& displayName, const std::string& content);
    ~MessageError() {};
    MessageType getType() override {return MessageType::ERR;};
    std::string getDisplayName() const { return displayName; };
    std::string getContent() const { return content; };
    std::string getTCPMsg() override;
    std::string getUDPMsg() override;
    
    protected:
    std::string displayName;
    std::string content;
};

// message Reply
class MessageReply : public Message {
    public:
    MessageReply(uint16_t msgID, bool isOk, uint16_t refID, const std::string& content);
    ~MessageReply() {};
    MessageType getType() override {return isOk ? MessageType::pREPLY : MessageType::nREPLY;};
    bool isReplyOk() const { return isOk; };
    std::string getContent() const { return content; };
    uint16_t getRefMsgID() const { return refMsgID; };
    std::string getTCPMsg() override;
    std::string getUDPMsg() override;
    
    protected:
    bool isOk;
    uint16_t refMsgID;
    std::string content;
};

// message Auth
class MessageAuth : public Message {
    public:
    MessageAuth(uint16_t msgID, const std::string& username, const std::string& displayName, const std::string& secret);
    ~MessageAuth() {};
    MessageType getType() override {return MessageType::AUTH;};
    std::string getTCPMsg() override;
    std::string getUDPMsg() override;
    
    protected:
    std::string username;
    std::string displayName;
    std::string secret;
};

// message Join
class MessageJoin : public Message {
    public:
    MessageJoin(uint16_t msgID, const std::string& channelId, const std::string& displayName);
    ~MessageJoin() {};
    MessageType getType() override {return MessageType::JOIN;};
    std::string getTCPMsg() override;
    std::string getUDPMsg() override;
    
    protected:
    std::string displayName;
    std::string channelId;
};

// message Msg
class MessageMsg : public Message {
    public:
    MessageMsg(uint16_t msgID, const std::string& displayName, const std::string& content);
    ~MessageMsg() {};
    MessageType getType() override {return MessageType::MSG;};
    std::string getDisplayName() const { return displayName; };
    std::string getContent() const { return content; };
    std::string getTCPMsg() override;
    std::string getUDPMsg() override;
    
    protected:
    std::string displayName;
    std::string content;
};

// message bye
class MessageBye : public Message {
    public:
    MessageBye(uint16_t msgID, const std::string& displayName);
    ~MessageBye() {};
    MessageType getType() override {return MessageType::BYE;};
    std::string getTCPMsg() override;
    std::string getUDPMsg() override;
    
    protected:
    std::string displayName;
};

// CONFIRM message
class MessageConfirm : public Message {
    public:
    MessageConfirm(uint16_t msgID) : Message(msgID) {};
    ~MessageConfirm() {};
    std::string getUDPMsg() override;
    std::string getTCPMsg() override {return "";};
    MessageType getType() override {return MessageType::CONFIRM;};
    
};

// PING message
class MessagePing : public Message {
    public:
    MessagePing(uint16_t msgID) : Message(msgID) {};
    ~MessagePing() {};
    std::string getUDPMsg() override;
    std::string getTCPMsg() override {return "";};
    MessageType getType() override {return MessageType::PING;};
    
};

#endif // MESSAGE_HPP