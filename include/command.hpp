
#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>
#include "utils.hpp"

/**
 * @class Command
 * @brief Abstract base class for handling user commands.
 *
 * This class provides an interface for command parsing and execution.
 * It includes utility functions such as help printing and dynamic
 * command creation based on user input.
 */
class Command {

    public:
        /**
         * @brief Default constructor.
         */
        Command() {};
    
        /**
         * @brief Virtual destructor.
         */
        virtual ~Command() {};
    
        /**
         * @brief Creates a new command instance based on user input.
         * @param userInput The raw input string from the user.
         * @return A pointer to a dynamically created Command object.
         */
        Command* createCommand(std::string userInput);
    
        /**
         * @brief Virtual method for representing the command.
         *
         * Can be overridden by derived classes to output specific
         * command-related information.
         */
        virtual void represent() {};
    
    private:
        /**
         * @brief Prints help information to the console.
         *
         * This function provides a brief usage message for commands.
         */
        void printHelp() const;
    };
    

/**
 * @class CommandAuth
 * @brief Derived class for handling authentication commands.
 *  
 * This class is responsible for parsing and storing the
 * username, secret, and display name from the user input.
 */
class CommandAuth : public Command {

    public:
        /**
         * @brief Constructor that initializes the command with user input.
         * @param userInput The raw input string from the user.
         */
        CommandAuth(std::string userInput);
        /**
         * @brief Destructor.
         */
        ~CommandAuth() {};
        /**
         * @brief Returns the username.
         * @return The username as a string.
         */
        std::string getUsername() const { return username; };
        /**
         * @brief Returns the secret.
         * @return The secret as a string.
         */
        std::string getSecret() const { return secret; };
        /**
         * @brief Returns the display name.
         * @return The display name as a string.
         */
        std::string getDisplayName() const { return displayName; };
        /**
         * @brief Represents the command.
         *
         * This method outputs the command-related information to the console.
         */
        void represent() override;
    private:
        // username, secret and display name
        std::string username;
        std::string secret;
        std::string displayName;
};

/**
 * @class CommandJoin
 * @brief Derived class for handling channel join commands.
 *
 * This class is responsible for parsing and storing the
 * channel ID from the user input.
 */
class CommandJoin : public Command {
    public:
        /**
         * @brief Constructor that initializes the command with user input.
         * @param userInput The raw input string from the user.
         */
        CommandJoin(std::string userInput);
        /**
         * @brief Destructor.
         */
        ~CommandJoin() {};
        /**
         * @brief Returns the channel ID.
         * @return The channel ID as a string.
         */
        std::string getChannelId() const { return channelId; };
        /**
         * @brief Represents the command.
         *
         * This method outputs the command-related information to the console.
         */
        void represent() override;
    private:
        // channel ID
        std::string channelId;
};

/**
 * @class CommandLeave
 * @brief Derived class for handling channel leave commands.
 *
 * This class is responsible for parsing and storing the
 * channel ID from the user input.
 */
class CommandRename : public Command {
    public:
        /**
         * @brief Constructor that initializes the command with user input.
         * @param userInput The raw input string from the user.
         */
        CommandRename(std::string userInput);
        /**
         * @brief Destructor.
         */
        ~CommandRename() {};
        /**
         * @brief Returns the new name.
         * @return The new name as a string.
         */
        std::string getNewName() const { return newName; };
        /**
         * @brief Represents the command.
         *
         * This method outputs the command-related information to the console.
         */
        void represent() override;
    private:
        // new name
        std::string newName;
};

/**
 * @class CommandMessage
 * @brief Derived class for handling message commands.
 *
 * This class is responsible for parsing and storing the
 * message content from the user input.
 */
class CommandMessage : public Command {
    public:
        /**
         * @brief Constructor that initializes the command with user input.
         * @param userInput The raw input string from the user.
         */
        CommandMessage(std::string userInput);
        /**
         * @brief Destructor.
         */
        ~CommandMessage() {};
        /**
         * @brief Returns the message content.
         * @return The message content as a string.
         */
        std::string getMessage() const { return message; };
        /**
         * @brief Represents the command.
         */
        void represent() override;
    private:
        // message content
        std::string message;
};

#endif // COMMAND_HPP