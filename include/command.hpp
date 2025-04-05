
#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>
#include "utils.hpp"

class Command {

    public:
        Command() {};
        ~Command() {};
        Command* createCommand(std::string userInput);
        virtual void represent() {};
    private:
        void printHelp() const;
};

class CommandAuth : public Command {

    public:
        CommandAuth(std::string userInput);
        ~CommandAuth() {};
        std::string getUsername() const { return username; };
        std::string getSecret() const { return secret; };
        std::string getDisplayName() const { return displayName; };
        void represent() override;
    private:
        std::string username;
        std::string secret;
        std::string displayName;
};

class CommandJoin : public Command {
    public:
        CommandJoin(std::string userInput);
        ~CommandJoin() {};
        std::string getChannelId() const { return channelId; };
        void represent() override;
    private:
        std::string channelId;
};

class CommandRename : public Command {
    public:
        CommandRename(std::string userInput);
        ~CommandRename() {};
        std::string getNewName() const { return newName; };
        void represent() override;
    private:
        std::string newName;
};

class CommandMessage : public Command {
    public:
        CommandMessage(std::string userInput);
        ~CommandMessage() {};
        std::string getMessage() const { return message; };
        void represent() override;
    private:
        std::string message;
};

#endif // COMMAND_HPP