/**
 * @file utils.hpp
 * @brief Header file for utility functions and enumerations
 * @author Martin Mendl <x247581>
 * @date 18.4.2025
*/

#ifndef UTILS_HPP
#define UTILS_HPP

#define MAX_EVENTS 2 
#define BUFFER_SIZE 65536

// Socket pair for signal handling
extern int sigfds[2]; 
void signalHandler(int sig);

// Enumeration for the status of UDP messages
enum class UDPmessaStatus {
    SENT,       // Message has been sent
    CONFIRMED,  // Message has been confirmed
};

// Enumeration for the finite state machine (FSM) states
enum class FSMState {
    START,  // Initial state
    AUTH,   // Authentication state
    OPEN,   // Open connection state
    JOIN,   // Join session state
    END,    // End state
};

// Enumeration for different types of messages
enum class MessageType {
    AUTH,      // Authentication message
    BYE,       // Goodbye message
    CONFIRM,   // Confirmation message (for UDP only)
    ERR,       // Error message
    JOIN,      // Join session message
    MSG,       // General message
    PING,      // Ping message (for UDP only)
    pREPLY,    // Positive reply
    nREPLY,    // Negative reply
    UNKNOWN    // Unknown message type
};

// Enumeration for the status of a client
enum class ClientStatus {
    AUTHENTICATED, // Client is authenticated
    JOINED,        // Client has joined a session
    NONE           // Client has no status
};

// Enumeration for the communication mode
enum class Mode {
    TCP,  // Transmission Control Protocol
    UDP,  // User Datagram Protocol
    NONE  // No mode specified
};

// Enumeration for the IP version
enum class IpVersion {
    IPV4, // Internet Protocol version 4
    IPV6, // Internet Protocol version 6
    None  // No IP version specified
};

/**
 * @enum TargetType
 * @brief Enumeration for different target types in scanning.
 */
enum class TargetType {
    IP_v4,       // IPv4 address
    IP_v6,       // IPv6 address
    DOMAIN_NAME, // Domain name
    UNKNOWN      // Unknown target type
};

#endif // UTILS_HPP