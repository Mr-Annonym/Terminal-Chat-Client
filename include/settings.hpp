

#ifndef SETTINGS_HPPP
#define SETTINGS_HPPP

#include <string>
#include "utils.hpp"


struct NetworkAdress {
    std::string hostName;
    std::string ip;
    IpVersion ipVer;
    uint16_t port;
};

TargetType determinTargetType(const std::string &target);

class Settings {

    public:
        Settings(int argc, char* argv[]);
        ~Settings() {};

        Mode getMode() const { return mode; };
        NetworkAdress getServer() const { return server; };
        int getUdpTimeoutConfirmation() const { return udpTimeoutConfirmation; };
        int getMaxUdpRetransmissions() const { return maxUdpRetransmissions; };
        void representSettings() const;

    private:
        Mode mode;
        NetworkAdress server;
        int udpTimeoutConfirmation;
        int maxUdpRetransmissions;
        void printHelp() const;
        std::string getIpFromDomain(const std::string& domain);
};



#endif // SETTINGS_HPPP