/**
 * @file main.cpp
 * @brief Main file for the chat application
 * @author Martin Mendl <x247581>
 * @date 18.4.2025
 */

#include <iostream>
#include "settings.hpp"
#include "chat.hpp"
#include "utils.hpp"

int main(int argc, char* argv[]) {

    // parse arguments
    Settings settings(argc, argv);
    //settings.representSettings(); // for debug

    Mode mode = settings.getMode();
    NetworkAdress server = settings.getServer();

    // no mode err
    if (mode == Mode::NONE) {
        std::cerr << "Error: No mode specified. Use -t tcp or -t udp.\n";
        return 1;
    }

    // tcp
    if (settings.getMode() == Mode::TCP) {
        ChatTCP chat(server);    
        chat.eventLoop();
    }

    // udp
    if (settings.getMode() == Mode::UDP) {
        ChatUDP chat(server, settings.getMaxUdpRetransmissions(), settings.getUdpTimeoutConfirmation());
        chat.eventLoop();
    }

    return 0;
}