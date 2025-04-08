#include <iostream>
#include "settings.hpp"
#include "chat.hpp"
#include "utils.hpp"

int main(int argc, char* argv[]) {

    Settings settings(argc, argv);
    //settings.representSettings();
    Mode mode = settings.getMode();

    NetworkAdress server = settings.getServer();

    if (mode == Mode::NONE) {
        std::cerr << "Error: No mode specified. Use -t tcp or -t udp.\n";
        return 1;
    }

    if (settings.getMode() == Mode::TCP) {
        ChatTCP chat(server);    
        chat.eventLoop();
    }

    if (settings.getMode() == Mode::UDP) {
        ChatUDP chat(server, settings.getMaxUdpRetransmissions(), settings.getUdpTimeoutConfirmation());
        chat.eventLoop();
    }

    return 0;
}