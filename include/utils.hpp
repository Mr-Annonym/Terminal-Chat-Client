
#ifndef UTILS_HPP
#define UTILS_HPP

#define MAX_EVENTS 2 
#define BUFFER_SIZE 65536
// Socket pair for signal handling
void signalHandler(int sig);

enum class UDPmessaStatus {
    SENT,
    CONFIRMED,
};

enum class FSMState {
    START,
    AUTH,
    OPEN,
    JOIN,
    END,
};

enum class MessageType {
    AUTH,
    BYE,
    CONFIRM, // for UDP only
    ERR,
    JOIN,
    MSG,
    PING, // for UDP only
    pREPLY, // positive reply
    nREPLY, // negative reply
    UNKNOWN
};

enum class ClientStatus {
    AUTHENTICATED,
    JOINED,
    NONE
};

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


#endif // UTILS_HPP