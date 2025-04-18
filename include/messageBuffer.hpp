/**
 * @file messageBuffer.hpp
 * @brief Header file for the MessageBuffer class
 * @author Martin Mendl <x247581>
 * @date 18.4.2025
*/
#ifndef MESSAGEBUFFER_HPP
#define MESSAGEBUFFER_HPP

#include <string>
#include <vector>

/**
 * @class MessageBuffer
 * @brief Class for managing a buffer of messages.
 *
 * This class provides methods to add, retrieve, and check the status of messages in a buffer.
 */
class MessageBuffer {

    public:
        /**
         * @brief Default constructor.
         */
        MessageBuffer();
        /**
         * @brief Destructor.
         */
        ~MessageBuffer();
        /**
         * @brief Adds a message to the buffer.
         * @param message The message to be added.
         */
        void addMessage(const std::string& message);
        /**
         * @brief Retrieves a message from the buffer.
         * @return The retrieved message.
         */
        std::string getMessage();
        /**
         * @brief Checks if the buffer is empty.
         * @return True if the buffer is empty, false otherwise.
         */
        bool isEmpty();
    private:
        std::vector<std::string> buffer; ///< Vector to store messages
};

#endif // MESSAGEBUFFER_HPP