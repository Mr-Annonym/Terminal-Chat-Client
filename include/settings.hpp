

#ifndef SETTINGS_HPPP
#define SETTINGS_HPPP

#include <string>

enum class Mode {
    TCP,
    UDP,
    NONE
};

enum class IpVersion {
    IPV4,
    IPV6,
    None
};

/**
 * @enum Mode
 * @brief Enumeration for different scanning modes.
 */
enum class TargetType {
    IP_v4,
    IP_v6,
    DOMAIN_NAME,
    UNKNOWN
};

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
};



#endif // SETTINGS_HPPP