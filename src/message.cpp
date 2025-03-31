
#include <regex>
#include <iostream>
#include "message.hpp"

bool startsWith(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;  // Check if found at position 0
}

Message* TCPMessages::readResponse(std::string resopnse) {
    if (startsWith(resopnse, "REPLY")) {
         std::regex pattern(R"(^\s*REPLY\s+(\S+)\s+IS\s+(.*)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string result = matches[1].str();
            std::string content = matches[2].str();
            bool isOk = (result == "OK");
            return new MessageReplyTCP(isOk, content);
        }
        return nullptr;
    }

    if (startsWith(resopnse, "ERR FROM")) {
        std::regex pattern(R"(^\s*ERR\s+FROM\s+(\S+)\s+IS\s+(.*)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string displayName = matches[1].str();
            std::string content = matches[2].str();
            return new MessageErrorTCP(displayName, content);
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
            return new MessageAuthTCP(username, displayName, secret);
        }
        return nullptr;
    }

    if (startsWith(resopnse, "JOIN")) {
        std::regex pattern(R"(^\s*JOIN\s+(\S+)\s+AS\s+(\S+)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string channelId = matches[1].str();
            std::string displayName = matches[2].str();
            return new MessageJoinTCP(channelId, displayName);
        }
        return nullptr;
    }

    if (startsWith(resopnse, "MSG FROM")) {
        std::regex pattern(R"(^\s*MSG\s+FROM\s+(\S+)\s+IS\s+(.*)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string displayName = matches[1].str();
            std::string content = matches[2].str();
            return new MessageMsgTCP(displayName, content);
        }
        return nullptr;
    }

    if (startsWith(resopnse, "BYE FROM")) {
        std::regex pattern(R"(^\s*BYE\s+FROM\s+(\S+)\r\n$)");
        std::smatch matches;
        if (std::regex_match(resopnse, matches, pattern)) {
            std::string displayName = matches[1].str();
            return new MessageByeTCP(displayName);
        }
        return nullptr;
    }
    return nullptr; // Unknown message type
}

unsigned int UDPMessages::getNextZeroIdx(std::string message, unsigned int startIdx) {
    std::size_t idx = message.find('\0', startIdx);
    if (idx == std::string::npos) {
        return message.size();
    }
    return static_cast<unsigned int>(idx);
}

Message* UDPMessages::readResponse(std::string resopnse) {

    // need to read the first byte to determine the message type
    uint16_t msgType = static_cast<uint16_t>(resopnse[0]);
    uint16_t msgID1 = (static_cast<uint16_t>(resopnse[1]) << 8) | static_cast<uint16_t>(resopnse[2]);
    unsigned int idx1, idx2, idx3;
    try {
        switch (msgType) {
            case 0x00: // CONFIRM
                return new MessageConfirm(msgID1); 
            case 0x01: // REPLY
                return new MessageReplyUDP(msgID1, static_cast<bool>(resopnse[3]), 
                    (static_cast<uint16_t>(resopnse[4]) << 8) | static_cast<uint16_t>(resopnse[5]),
                    resopnse.substr(6, getNextZeroIdx(resopnse, 6) - 1));
            case 0x02: // AUTH
                idx1 = getNextZeroIdx(resopnse, 3);
                idx2 = getNextZeroIdx(resopnse, idx1 + 1);
                idx3 = getNextZeroIdx(resopnse, idx2 + 1);
                return new MessageAuthUDP(
                    msgID1, 
                    resopnse.substr(3, idx1 - 1),
                    resopnse.substr(idx1 + 1, idx2 - 1),
                    resopnse.substr(idx2 + 1, idx3 - 1)
                );
            case 0x03: // JOIN
                idx1 = getNextZeroIdx(resopnse, 3);
                idx2 = getNextZeroIdx(resopnse, idx1 + 1);
                return new MessageJoinUDP(
                    msgID1, 
                    resopnse.substr(3, idx1 - 1),
                    resopnse.substr(idx1 + 1, idx2 - 1)
                );
            case 0x04: // MSG
                idx1 = getNextZeroIdx(resopnse, 3);
                idx2 = getNextZeroIdx(resopnse, idx1 + 1);
                return new MessageMsgUDP(
                    msgID1, 
                    resopnse.substr(3, idx1 - 1),
                    resopnse.substr(idx1 + 1, idx2 - 1)
                );
            case 0xFD: // PING
                return new MessagePing(msgID1);
            case 0xFE: // ERR
                idx1 = getNextZeroIdx(resopnse, 3);
                idx2 = getNextZeroIdx(resopnse, idx1 + 1);
                return new MessageErrorUDP(
                    msgID1, 
                    resopnse.substr(3, idx1 - 1),
                    resopnse.substr(idx1 + 1, idx2 - 1)
                );
            case 0xFF: // BYE
                idx1 = getNextZeroIdx(resopnse, 3);
                return new MessageByeUDP(
                    msgID1, 
                    resopnse.substr(3, idx1 - 1)
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
        return new MessageAuthUDP(messageID, client->username, client->displayName, client->secret);
    }

    // join
    if (typeid(*command) == typeid(CommandJoin)) {
        CommandJoin* joinCmd = dynamic_cast<CommandJoin*>(command);
        client->currentChannel = joinCmd->getChannelId();
        return new MessageJoinUDP(messageID, client->currentChannel, client->displayName);
    }

    // msg
    if (typeid(*command) == typeid(CommandMessage)) {
        CommandMessage* msgCmd = dynamic_cast<CommandMessage*>(command);
        return new MessageMsgUDP(messageID, client->displayName, msgCmd->getMessage());
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
        return new MessageAuthTCP(client->username, client->displayName, client->secret);
    }

    // join
    if (typeid(*command) == typeid(CommandJoin)) {
        CommandJoin* joinCmd = dynamic_cast<CommandJoin*>(command);
        client->currentChannel = joinCmd->getChannelId();
        return new MessageJoinTCP(client->currentChannel, client->displayName);
    }

    // message
    if (typeid(*command) == typeid(CommandMessage)) {
        CommandMessage* msgCmd = dynamic_cast<CommandMessage*>(command);
        return new MessageMsgTCP(client->displayName, msgCmd->getMessage());
    }

    return nullptr; // not a valid command to generate
}

// ERR message
MessageError::MessageError(std::string displayName, std::string content) {
    this->displayName = displayName;
    this->content = content;
}

// Constructor for MessageErrorUDP
MessageErrorUDP::MessageErrorUDP(
    uint16_t msgID, 
    std::string displayName, 
    std::string content
) : MessageError(displayName, content) {
    this->msgID = msgID;
}

/*
  1 byte       2 bytes
+--------+--------+--------+-------~~------+---+--------~~---------+---+
|  0xFE  |    MessageID    |  DisplayName  | 0 |  MessageContents  | 0 |
+--------+--------+--------+-------~~------+---+--------~~---------+---+
*/
std::string MessageErrorUDP::getMessage() {
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
std::string MessageErrorTCP::getMessage() {
    std::string message = "ERR FROM " + displayName + " IS " + content + "\r\n";
    return message;
}

// REPLY message
MessageReply::MessageReply(bool isOk, std::string content) {
    this->isOk = isOk;
    this->content = content;
}

// Constructor for MessageReplyUDP
MessageReplyUDP::MessageReplyUDP(
    uint16_t msgID, 
    bool isOk, 
    uint16_t refMsgID, 
    std::string content
) : MessageReply(isOk, content) {
    this->msgID = msgID;
    this->refMsgID = refMsgID;
}

/*
  1 byte       2 bytes       1 byte       2 bytes      
+--------+--------+--------+--------+--------+--------+--------~~---------+---+
|  0x01  |    MessageID    | Result |  Ref_MessageID  |  MessageContents  | 0 |
+--------+--------+--------+--------+--------+--------+--------~~---------+---+
*/
std::string MessageReplyUDP::getMessage() {
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
std::string MessageReplyTCP::getMessage() {
    std::string message = "REPLY " + std::string(isOk ? "OK" : "NOK") + " IS " + content + "\r\n";
    return message;
}

// AUTH message
MessageAuth::MessageAuth(std::string username, std::string displayName, std::string secret) {
    this->username = username;
    this->secret = secret;
    this->displayName = displayName;
}

// Constructor for MessageAuthUDP
MessageAuthUDP::MessageAuthUDP(
    uint16_t msgID, 
    std::string username, 
    std::string displayName, 
    std::string secret
) : MessageAuth(username, displayName, secret) {
    this->msgID = msgID;
}

/*
  1 byte       2 bytes      
+--------+--------+--------+-----~~-----+---+-------~~------+---+----~~----+---+
|  0x02  |    MessageID    |  Username  | 0 |  DisplayName  | 0 |  Secret  | 0 |
+--------+--------+--------+-----~~-----+---+-------~~------+---+----~~----+---+
*/
std::string MessageAuthUDP::getMessage() {
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
std::string MessageAuthTCP::getMessage() {
    std::string message = "AUTH " + username + " AS " + displayName + " USING " + secret + "\r\n";
    return message;
}

// JOIN message
MessageJoin::MessageJoin(std::string channelId, std::string displayName) {
    this->channelId = channelId;
    this->displayName = displayName;
}

// Constructor for MessageJoinUDP
MessageJoinUDP::MessageJoinUDP(
    uint16_t msgID, 
    std::string channelId, 
    std::string displayName
) : MessageJoin(channelId, displayName) {
    this->msgID = msgID;
}

/*
  1 byte       2 bytes      
+--------+--------+--------+-----~~-----+---+-------~~------+---+
|  0x03  |    MessageID    |  ChannelID | 0 |  DisplayName  | 0 |
+--------+--------+--------+-----~~-----+---+-------~~------+---+
*/
std::string MessageJoinUDP::getMessage() {
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
std::string MessageJoinTCP::getMessage() {
    std::string message = "JOIN " + channelId + " AS " + displayName + "\r\n";
    return message;
}

// MSG message
MessageMsg::MessageMsg(std::string displayName, std::string content) {
    this->displayName = displayName;
    this->content = content;
}

// Constructor for MessageMsgUDP
MessageMsgUDP::MessageMsgUDP(
    uint16_t msgID, 
    std::string displayName, 
    std::string content
) : MessageMsg(displayName, content) {
    this->msgID = msgID;
}

/*
  1 byte       2 bytes      
+--------+--------+--------+-------~~------+---+--------~~---------+---+
|  0x04  |    MessageID    |  DisplayName  | 0 |  MessageContents  | 0 |
+--------+--------+--------+-------~~------+---+--------~~---------+---+
*/
std::string MessageMsgUDP::getMessage() {
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
std::string MessageMsgTCP::getMessage() {
    std::string message = "MSG FROM " + displayName + " IS " + content + "\r\n";
    return message;
}

// BYE message
MessageBye::MessageBye(std::string displayName) {
    this->displayName = displayName;
}

// Constructor for MessageByeUDP
MessageByeUDP::MessageByeUDP(
    uint16_t msgID, 
    std::string displayName
) : MessageBye(displayName) {
    this->msgID = msgID;
}

/*
  1 byte       2 bytes
+--------+--------+--------+-------~~------+---+
|  0xFF  |    MessageID    |  DisplayName  | 0 |
+--------+--------+--------+-------~~------+---+
*/
std::string MessageByeUDP::getMessage() {
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
std::string MessageByeTCP::getMessage() {
    std::string message = "BYE FROM " + displayName + "\r\n";
    return message;
}

/*
  1 byte       2 bytes      
+--------+--------+--------+
|  0x00  |  Ref_MessageID  |
+--------+--------+--------+
*/
std::string MessageConfirm::getMessage() {
    std::string message;
    message.reserve(1 + 2); // Preallocate
    message.push_back(static_cast<char>(0x00));  // Protocol identifier
    message.push_back(static_cast<char>((ID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(ID & 0xFF));        // Low byte of msgID
    return message;

}

/*
  1 byte       2 bytes
+--------+--------+--------+
|  0xFD  |    MessageID    |
+--------+--------+--------+
*/
std::string MessagePing::getMessage() {
    std::string message;
    message.reserve(1 + 2); // Preallocate
    message.push_back(static_cast<char>(0xFD));  // Protocol identifier
    message.push_back(static_cast<char>((ID >> 8) & 0xFF)); // High byte of msgID
    message.push_back(static_cast<char>(ID & 0xFF));        // Low byte of msgID
    return message;
}