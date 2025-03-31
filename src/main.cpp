#include <iostream>
#include "settings.hpp"
#include "chat.hpp"

int main(int argc, char* argv[]) {

    Settings settings(argc, argv);
    settings.representSettings();

    NetworkAdress server = settings.getServer();

    ChatTCP chat(server);    
    std::cout << "Chat start\n";
    chat.eventLoop();

    return 0;
}