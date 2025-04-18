/**
 * @file messageBuffer.cpp
 * @brief Implementation of the MessageBuffer class
 * @author Martin Mendl <x247581>
 * @date 18.4.2025
*/

#include "messageBuffer.hpp"

// Constructor
MessageBuffer::MessageBuffer() {
    this->buffer = std::vector<std::string>();
}

// Destructor
MessageBuffer::~MessageBuffer() {
    this->buffer.clear();
}

// Add message to buffer
void MessageBuffer::addMessage(const std::string& message) {
    this->buffer.push_back(message);
}

// Get message from buffer
std::string MessageBuffer::getMessage() {
    if (this->buffer.empty()) return "";
    std::string message = this->buffer.front();
    this->buffer.erase(this->buffer.begin());
    return message;
}

// Check if buffer is empty
bool MessageBuffer::isEmpty() {
    return this->buffer.empty();
}


