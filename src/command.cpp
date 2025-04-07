
#include <iostream>
#include <string>
#include <regex>
#include <stdexcept>
#include "command.hpp"

Command* Command::createCommand(std::string userInput) {
    // check if the userInput is empty
    if (userInput.empty()) {
        return nullptr;
    }
    try {
        // Check if the userInput starts with a '/'
        if (userInput.find("/") == 0) {
            // Match specific commands
            if (userInput.find("/auth") == 0) return new CommandAuth(userInput);
            if (userInput.find("/join") == 0) return new CommandJoin(userInput);
            if (userInput.find("/rename") == 0) return new CommandRename(userInput);
            if (userInput.find("/help") == 0) {
                printHelp();
                return nullptr;
            }
            // If none of the commands match, it's an invalid command
            std::cerr << "Error: Invalid command. Type /help for a list of commands.\n";
            return nullptr;
        }
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << "\n";
        return nullptr;
    }
    // If the input does not start with '/', treat it as a message
    return new CommandMessage(userInput);
}

void Command::printHelp() const {
    std::cout << "+----------+----------------------------------+------------------------------------------+\n";
    std::cout << "| Command  | Parameters                       | Description                              |\n";
    std::cout << "+----------+----------------------------------+------------------------------------------+\n";
    std::cout << "| /auth    | {Username} {Secret} {DisplayName}| Authenticate user with credentials       |\n";
    std::cout << "| /join    | {ChannelID}                      | Join a specific channel                  |\n";
    std::cout << "| /rename  | {DisplayName}                    | Change the display name                  |\n";
    std::cout << "| /help    |                                  | Display the list of available commands   |\n";
    std::cout << "+----------+----------------------------------+------------------------------------------+\n";
}

// Constructor for CommandAuth
CommandAuth::CommandAuth(std::string userInput) {
    // Define a regex pattern to match the /auth command followed by Username, Secret, and DisplayName
    std::regex pattern(R"(^\s*/auth\s+(\S+)\s+(\S+)\s+(\S.*?)(\s+.*)?$)");
    std::smatch matches;

    // Check if the input matches the pattern
    if (std::regex_match(userInput, matches, pattern)) {
        if (matches[4].matched) {
            // If there is extra input after the expected parameters, throw an error
            throw std::invalid_argument("Error: Too many arguments for /auth command.");
        }
        // Extract the Username, Secret, and DisplayName from the capturing groups
        username = matches[1].str();
        secret = matches[2].str();
        displayName = matches[3].str();
        return;
    }

    // If the input doesn't match the expected format, throw an error
    throw std::invalid_argument("Error: Invalid format for /auth command.");
}

void CommandAuth::represent() {
    std::cout << "Command: AUTH\n";
    std::cout << "Username: " << username << "\n";
    std::cout << "Secret: " << secret << "\n";
    std::cout << "Display Name: " << displayName << "\n";
}


// Constructor for CommandJoin
CommandJoin::CommandJoin(std::string userInput) {
    // Define a regex pattern to match the /join command followed by a single string ChannelID
    std::regex pattern(R"(^\s*/join\s+(\S+)(\s+.*)?$)");
    std::smatch matches;

    // Check if the input matches the pattern
    if (std::regex_match(userInput, matches, pattern)) {
        if (matches[2].matched) {
            // If there is extra input after the ChannelID, throw an error
            throw std::invalid_argument("Error: Too many arguments for /join command.");
        }
        // Extract the ChannelID from the first capturing group
        channelId = matches[1].str();
        return;
    }

    // If the input doesn't match the expected format, throw an error
    throw std::invalid_argument("Error: Invalid format for /join command.");
}

void CommandJoin::represent() {
    std::cout << "Command: JOIN\n";
    std::cout << "Channel ID: " << channelId << "\n";
}

// Constructor for CommandRename
CommandRename::CommandRename(std::string userInput) {
    // Define a regex pattern to match the /rename command followed by a single word or phrase
    std::regex pattern(R"(^\s*/rename\s+(\S.*?)(\s+.*)?$)");
    std::smatch matches;

    // Check if the input matches the pattern
    if (std::regex_match(userInput, matches, pattern)) {
        if (matches[2].matched) {
            // If there is extra input after the new name, throw an error
            throw std::invalid_argument("Error: Too many arguments for /rename command.");
        }
        // Extract the new name from the first capturing group
        newName = matches[1].str();
        return;
    } 

    // If the input doesn't match the expected format, throw an error
    throw std::invalid_argument("Error: Invalid format for /rename command.");
}

void CommandRename::represent() {
    std::cout << "Command: RENAME\n";
    std::cout << "New Name: " << newName << "\n";
}

CommandMessage::CommandMessage(std::string userInput) {
    if (userInput.empty()) {
        throw std::invalid_argument("Error: Empty message.");
    }
    // Define a regex pattern to match the message command
    message = userInput;
}

void CommandMessage::represent() {
    std::cout << "Command: MESSAGE\n";
    std::cout << "Message: " << message << "\n";
}