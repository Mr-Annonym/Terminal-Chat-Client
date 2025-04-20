/**
 * @file message.cpp
 * @brief Implementation of the Message class and its derived classes
 * @author Martin Mendl <x247581>
 * @date 18.4.2025
 */
#include <regex>
#include <iostream>
#include "message.hpp"

// function to findt out if a string starts with a prefix
bool startsWith(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;  // Check if found at position 0
}

// TCP factory method, to read a response
Message* TCPMessages::readResponse(const std::string &resopnse) {
    if (startsWith(resopnse, "REPLY")) {
        std::regex pattern(R"(^\s*REPLY\s+(\S+)\s+IS\s+(.*)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string result = matches[1].str();
            std::string content = matches[2].str();
            bool isOk = (result == "OK");
            return new MessageReply(0, isOk, 0, content);
        }
        return nullptr;
    }

    if (startsWith(resopnse, "ERR FROM")) {
        std::regex pattern(R"(^\s*ERR\s+FROM\s+(\S+)\s+IS\s+(.*)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string displayName = matches[1].str();
            std::string content = matches[2].str();
            return new MessageError(0, displayName, content);
        }
        return nullptr;
    }

    if (startsWith(resopnse, "AUTH")) {
        std::regex pattern(R"(^\s*AUTH\s+(\S+)\s+AS\s+(\S+)\s+USING\s+(.*)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string username = matches[1].str();
            std::string displayName = matches[2].str();
            std::string secret = matches[3].str();
            return new MessageAuth(0, username, displayName, secret);
        }
        return nullptr;
    }

    if (startsWith(resopnse, "JOIN")) {
        std::regex pattern(R"(^\s*JOIN\s+(\S+)\s+AS\s+(\S+)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string channelId = matches[1].str();
            std::string displayName = matches[2].str();
            return new MessageJoin(0, channelId, displayName);
        }
        return nullptr;
    }

    if (startsWith(resopnse, "MSG FROM")) {
        std::regex pattern(R"(^\s*MSG\s+FROM\s+(\S+)\s+IS\s+(.*)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string displayName = matches[1].str();
            std::string content = matches[2].str();
            return new MessageMsg(0, displayName, content);
        }
        return nullptr;
    }

    if (startsWith(resopnse, "BYE FROM")) {
        std::regex pattern(R"(^\s*BYE\s+FROM\s+(\S+)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string displayName = matches[1].str();
            return new MessageBye(0, displayName);
        }
        return nullptr;
    }
    return nullptr; // Unknown message type
}

// method to get the next index of a x00 byte in a string
std::size_t UDPMessages::getNextZeroIdx(const std::string& message, std::size_t startIdx) {
    std::size_t idx = message.find('\0', startIdx);
    if (idx == std::string::npos) {
        return message.size();
    }
    return idx;
}

// factory method for udp (converting packets to Message objects)
Message* UDPMessages::readResponse(const std::string& resopnse) {

    // need to read the first byte to determine the message type
    uint8_t msgType = static_cast<uint8_t>(resopnse[0]);
    uint16_t msgID1 = (static_cast<uint16_t>(resopnse[1]) << 8) | static_cast<uint16_t>(resopnse[2]);
    // print out mesage type and msgID
    std::size_t idx1, idx2, idx3;
    try {
        switch (msgType) {
            case 0x00: // CONFIRM
                return new MessageConfirm(msgID1); 
            case 0x01: // REPLY
                return new MessageReply(msgID1, static_cast<bool>(resopnse[3]), 
                    (static_cast<uint16_t>(resopnse[4]) << 8) | static_cast<uint16_t>(resopnse[5]),
                    resopnse.substr(6, getNextZeroIdx(resopnse, 6) - 6));
            case 0x02: // AUTH
                idx1 = getNextZeroIdx(resopnse, 3);
                idx2 = getNextZeroIdx(resopnse, idx1 + 1);
                idx3 = getNextZeroIdx(resopnse, idx2 + 1);
                return new MessageAuth(
                    msgID1, 
                    resopnse.substr(3, idx1 - 3),
                    resopnse.substr(idx1 + 1, idx2 - idx1 - 1),
                    resopnse.substr(idx2 + 1, idx3 - idx2 - 1)
                );
            case 0x03: // JOIN
                idx1 = getNextZeroIdx(resopnse, 3);
                idx2 = getNextZeroIdx(resopnse, idx1 + 1);
                return new MessageJoin(
                    msgID1, 
                    resopnse.substr(3, idx1 - 3),
                    resopnse.substr(idx1 + 1, idx2 - idx1 - 1)
                );
            case 0x04: // MSG
                idx1 = getNextZeroIdx(resopnse, 3);
                idx2 = getNextZeroIdx(resopnse, idx1 + 1);
                return new MessageMsg(
                    msgID1, 
                    resopnse.substr(3, idx1 - 3),
                    resopnse.substr(idx1 + 1, idx2 - idx1 - 1)
                );
            case 0xFD: // PING
                return new MessagePing(msgID1);
            case 0xFE: // ERR
                idx1 = getNextZeroIdx(resopnse, 3);
                idx2 = getNextZeroIdx(resopnse, idx1 + 1);
                return new MessageError(
                    msgID1, 
                    resopnse.substr(3, idx1 - 3),
                    resopnse.substr(idx1 + 1, idx2 - idx1 - 1)
                );
            case 0xFF: // BYE
                idx1 = getNextZeroIdx(resopnse, 3);
                return new MessageBye(
                    msgID1, 
                    resopnse.substr(3, idx1 - 3)
                );
            default:
                return nullptr; // Unknown message type
        }
    }
    // malformed message | message with wrong formatting
    catch (const std::out_of_range& e) {
        return nullptr; 
    }
}

// Factory method for creating a message based on a user cmd udp
Message* UDPMessages::convertCommandToMessage(uint16_t messageID, Command* command) {

    // auth
    if (typeid(*command) == typeid(CommandAuth)) {
        CommandAuth* authCmd = dynamic_cast<CommandAuth*>(command);
        client->displayName = authCmd->getDisplayName();
        client->username = authCmd->getUsername();
        client->secret = authCmd->getSecret();
        return new MessageAuth(messageID, client->username, client->displayName, client->secret);
    }

    // join
    if (typeid(*command) == typeid(CommandJoin)) {
        CommandJoin* joinCmd = dynamic_cast<CommandJoin*>(command);
        client->currentChannel = joinCmd->getChannelId();
        return new MessageJoin(messageID, client->currentChannel, client->displayName);
    }

    // msg
    if (typeid(*command) == typeid(CommandMessage)) {
        CommandMessage* msgCmd = dynamic_cast<CommandMessage*>(command);
        return new MessageMsg(messageID, client->displayName, msgCmd->getMessage());
    }

    return nullptr; // not a valid cmd
}

// Factory method for creating a message based on a user cmd tcp
Message* TCPMessages::convertCommandToMessage(Command* command) {

    // auth
    if (typeid(*command) == typeid(CommandAuth)) {
        CommandAuth* authCmd = dynamic_cast<CommandAuth*>(command);
        client->displayName = authCmd->getDisplayName();
        client->username = authCmd->getUsername();
        client->secret = authCmd->getSecret();
        return new MessageAuth(0, client->username, client->displayName, client->secret);
    }

    // join
    if (typeid(*command) == typeid(CommandJoin)) {
        CommandJoin* joinCmd = dynamic_cast<CommandJoin*>(command);
        client->currentChannel = joinCmd->getChannelId();
        return new MessageJoin(0, client->currentChannel, client->displayName);
    }

    // message
    if (typeid(*command) == typeid(CommandMessage)) {
        CommandMessage* msgCmd = dynamic_cast<CommandMessage*>(command);
        return new MessageMsg(0, client->displayName, msgCmd->getMessage());
    }

    return nullptr; // not a valid command to generate
}

// ERR message
MessageError::MessageError(uint16_t msgID, const std::string& displayName, const std::string& content) : Message(msgID) {
    this->displayName = displayName;
    this->content = content;
}


/*
  1 byte       2 bytes
+--------+--------+--------+-------~~------+---+--------~~---------+---+
|  0xFE  |    MessageID    |  DisplayName  | 0 |  MessageContents  | 0 |
+--------+--------+--------+-------~~------+---+--------~~---------+---+
*/
std::string MessageError::getUDPMsg() {
    std::string message;
    message.reserve(1 + 1 + displayName.size() + 1 + content.size() + 1); // Preallocate
    message.push_back(static_cast<char>(0xFE));  // Protocol identifier
    message.push_back(static_cast<char>((msgID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(msgID & 0xFF));        // Low byte of msgID
    message.append(displayName);
    message.push_back('\0'); // Null-terminate display name
    message.append(content);
    message.push_back('\0'); // Null-terminate content
    return message;
}

// ERR FROM {DisplayName} IS {MessageContent}\r\n
std::string MessageError::getTCPMsg() {
    std::string message = "ERR FROM " + displayName + " IS " + content + "\r\n";
    return message;
}

// REPLY message
MessageReply::MessageReply(uint16_t msgID, bool isOk, uint16_t refID, const std::string& content) : Message(msgID) {
    this->isOk = isOk;
    this->content = content;
    this->refMsgID = refID;
}

/*
  1 byte       2 bytes       1 byte       2 bytes      
+--------+--------+--------+--------+--------+--------+--------~~---------+---+
|  0x01  |    MessageID    | Result |  Ref_MessageID  |  MessageContents  | 0 |
+--------+--------+--------+--------+--------+--------+--------~~---------+---+
*/
std::string MessageReply::getUDPMsg() {
    std::string message;
    message.reserve(1 + 2 + 1 + 2 + content.size() + 1); // Preallocate
    message.push_back(static_cast<char>(0x01));  // Protocol identifier
    message.push_back(static_cast<char>((msgID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(msgID & 0xFF));        // Low byte of msgID
    message.push_back(static_cast<char>(isOk ? 0x01 : 0x00)); // Result (1 byte)
    message.push_back(static_cast<char>((refMsgID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(refMsgID & 0xFF));        // Low byte of msgID
    message.append(content);
    message.push_back('\0'); // Null-terminate content
    return message;
}

// REPLY {"OK"|"NOK"} IS {MessageContent}\r\n
std::string MessageReply::getTCPMsg() {
    std::string message = "REPLY " + std::string(isOk ? "OK" : "NOK") + " IS " + content + "\r\n";
    return message;
}

// AUTH message
MessageAuth::MessageAuth(uint16_t msgID, const std::string& username, const std::string& displayName, const std::string& secret) : Message(msgID) {
    this->username = username;
    this->secret = secret;
    this->displayName = displayName;
}

/*
  1 byte       2 bytes      
+--------+--------+--------+-----~~-----+---+-------~~------+---+----~~----+---+
|  0x02  |    MessageID    |  Username  | 0 |  DisplayName  | 0 |  Secret  | 0 |
+--------+--------+--------+-----~~-----+---+-------~~------+---+----~~----+---+
*/
std::string MessageAuth::getUDPMsg() {
    std::string message;
    message.reserve(1 + 2 + username.size() + 1 + displayName.size() + 1 + secret.size() + 1); // Preallocate
    message.push_back(static_cast<char>(0x02));  // Protocol identifier
    message.push_back(static_cast<char>((msgID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(msgID & 0xFF));        // Low byte of msgID
    message.append(username);
    message.push_back('\0'); // Null-terminate username
    message.append(displayName);
    message.push_back('\0'); // Null-terminate display name
    message.append(secret);
    message.push_back('\0'); // Null-terminate secret
    return message;
}

// AUTH {Username} AS {DisplayName} USING {Secret}\r\n
std::string MessageAuth::getTCPMsg() {
    std::string message = "AUTH " + username + " AS " + displayName + " USING " + secret + "\r\n";
    return message;
}

// JOIN message
MessageJoin::MessageJoin(uint16_t msgID, const std::string& channelId, const std::string& displayName) : Message(msgID) {
    this->channelId = channelId;
    this->displayName = displayName;
}

/*
  1 byte       2 bytes      
+--------+--------+--------+-----~~-----+---+-------~~------+---+
|  0x03  |    MessageID    |  ChannelID | 0 |  DisplayName  | 0 |
+--------+--------+--------+-----~~-----+---+-------~~------+---+
*/
std::string MessageJoin::getUDPMsg() {
    std::string message;
    message.reserve(1 + 2 + channelId.size() + 1 + displayName.size() + 1); // Preallocate
    message.push_back(static_cast<char>(0x03));  // Protocol identifier
    message.push_back(static_cast<char>((msgID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(msgID & 0xFF));        // Low byte of msgID
    message.append(channelId);
    message.push_back('\0'); // Null-terminate channel ID
    message.append(displayName);
    message.push_back('\0'); // Null-terminate display name
    return message;
}

// JOIN {ChannelID} AS {DisplayName}\r\n
std::string MessageJoin::getTCPMsg() {
    std::string message = "JOIN " + channelId + " AS " + displayName + "\r\n";
    return message;
}   

// MSG message
MessageMsg::MessageMsg(uint16_t msgID, const std::string& displayName, const std::string& content) : Message(msgID) {
    this->displayName = displayName;
    this->content = content;
}

/*
  1 byte       2 bytes      
+--------+--------+--------+-------~~------+---+--------~~---------+---+
|  0x04  |    MessageID    |  DisplayName  | 0 |  MessageContents  | 0 |
+--------+--------+--------+-------~~------+---+--------~~---------+---+
*/
std::string MessageMsg::getUDPMsg() {
    std::string message;
    message.reserve(1 + 2 + displayName.size() + 1 + content.size() + 1); // Preallocate
    message.push_back(static_cast<char>(0x04));  // Protocol identifier
    message.push_back(static_cast<char>((msgID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(msgID & 0xFF));        // Low byte of msgID
    message.append(displayName);
    message.push_back('\0'); // Null-terminate display name
    message.append(content);
    message.push_back('\0'); // Null-terminate content
    return message;
}

// MSG FROM {DisplayName} IS {MessageContent}\r\n
std::string MessageMsg::getTCPMsg() {
    std::string message = "MSG FROM " + displayName + " IS " + content + "\r\n";
    return message;
}

// BYE message
MessageBye::MessageBye(uint16_t msgId, const std::string& displayName) : Message(msgId) {
    this->displayName = displayName;
}

/*
  1 byte       2 bytes
+--------+--------+--------+-------~~------+---+
|  0xFF  |    MessageID    |  DisplayName  | 0 |
+--------+--------+--------+-------~~------+---+
*/
std::string MessageBye::getUDPMsg() {
    std::string message;
    message.reserve(1 + 2 + displayName.size() + 1); // Preallocate
    message.push_back(static_cast<char>(0xFF));  // Protocol identifier
    message.push_back(static_cast<char>((msgID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(msgID & 0xFF));        // Low byte of msgID
    message.append(displayName);
    message.push_back('\0'); // Null-terminate display name
    return message;
}

// BYE FROM {DisplayName}\r\n
std::string MessageBye::getTCPMsg() {
    std::string message = "BYE FROM " + displayName + "\r\n";
    return message;
}

/*
  1 byte       2 bytes      
+--------+--------+--------+
|  0x00  |  Ref_MessageID  |
+--------+--------+--------+
*/
std::string MessageConfirm::getUDPMsg() {
    std::string message;
    message.reserve(1 + 2); // Preallocate
    message.push_back(static_cast<char>(0x00));  // Protocol identifier
    message.push_back(static_cast<char>((msgID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(msgID & 0xFF));        // Low byte of msgID
    return message;
}

/*
  1 byte       2 bytes
+--------+--------+--------+
|  0xFD  |    MessageID    |
+--------+--------+--------+
*/
std::string MessagePing::getUDPMsg() {
    std::string message;
    message.reserve(1 + 2); // Preallocate
    message.push_back(static_cast<char>(0xFD));  // Protocol identifier
    message.push_back(static_cast<char>((msgID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(msgID & 0xFF));        // Low byte of msgID
    return message;
}