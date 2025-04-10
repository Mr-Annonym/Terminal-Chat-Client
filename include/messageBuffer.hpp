
#ifndef MESSAGEBUFFER_HPP
#define MESSAGEBUFFER_HPP

#include <string>
#include <vector>

class MessageBuffer {

    public:
        MessageBuffer();
        ~MessageBuffer();
        void addMessage(const std::string& message);
        std::string getMessage();
        bool isEmpty();
    private:
        std::vector<std::string> buffer;

};

#endif // MESSAGEBUFFER_HPP