
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <regex>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "settings.hpp"

// Function to determine the target type
TargetType determinTargetType(const std::string &target) { 
    // Regular expressions for IPv4, IPv6, and domain name
    std::regex ipv4_regex("^(\\d{1,3}\\.){3}\\d{1,3}$");
    std::regex ipv6_regex("^[0-9a-fA-F:]+$");
    std::regex domain_regex("^[a-zA-Z0-9.-]+$"); 
    if (std::regex_match(target, ipv4_regex)) return TargetType::IP_v4;
    if (std::regex_match(target, ipv6_regex)) return TargetType::IP_v6;
    if (std::regex_match(target, domain_regex)) return TargetType::DOMAIN_NAME;
    return TargetType::UNKNOWN;
} 

std::string Settings::getIpFromDomain(const std::string& domain) {
    struct addrinfo hints{}, *res, *p;
    char ipStr[INET_ADDRSTRLEN]; // Buffer for IPv4 address

    hints.ai_family = AF_INET; // Only look for IPv4 addresses
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(domain.c_str(), nullptr, &hints, &res) != 0) {
        return ""; // Return empty string if resolution fails
    }

    for (p = res; p != nullptr; p = p->ai_next) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, sizeof(ipStr));
        freeaddrinfo(res); // Free memory
        return std::string(ipStr); // Return first IPv4 address
    }

    freeaddrinfo(res);
    return ""; // No IPv4 address found
}

Settings::Settings(int argc, char* argv[]) {
    // Default values for optional arguments

    memset(&server, 0, sizeof(server));
    server.ipVer = IpVersion::None; // indicate not set
    server.port = 4567; // default port

    udpTimeoutConfirmation = 250; // default timeout
    maxUdpRetransmissions = 3; // default max retransmissions
    mode = Mode::NONE; // default mode

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-t" && i + 1 < argc) {
            std::string protocol = argv[++i];
            if (protocol != "tcp" && protocol != "udp") {
                throw std::invalid_argument("Invalid value for -t. Expected 'tcp' or 'udp'.");
            }
            mode = (protocol == "tcp") ? Mode::TCP : Mode::UDP;
            continue;   
        }
        
        if (arg == "-s" && i + 1 < argc) {
            std::string serverArg = argv[++i];
            TargetType result = determinTargetType(serverArg);
            if (result == TargetType::UNKNOWN) {
                throw std::invalid_argument("Invalid server address: " + serverArg);
            }

            // ipv4
            if (result == TargetType::IP_v4) {
                server.ip = serverArg;
                server.ipVer = IpVersion::IPV4;
                continue;
            }

            // ipv6
            if (result == TargetType::IP_v6) {
                server.ip = serverArg;
                server.ipVer = IpVersion::IPV6;
                continue;
            }
            
            // domain name
            server.hostName = serverArg;
            server.ipVer = IpVersion::IPV4;
            server.ip = getIpFromDomain(serverArg);
            if (server.ip.empty()) throw std::invalid_argument("Could not resolve domain: " + serverArg);
            continue;
        }
        if (arg == "-p" && i + 1 < argc) {
            server.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            continue;

        } 
        if (arg == "-d" && i + 1 < argc) {
            udpTimeoutConfirmation = static_cast<uint16_t>(std::stoi(argv[++i]));
            continue;
        } 
        if (arg == "-r" && i + 1 < argc) {
            maxUdpRetransmissions = static_cast<uint8_t>(std::stoi(argv[++i]));
            continue;
        } 
        if (arg == "-h") {
            printHelp();
            std::exit(0);
            continue;   
        } 
        throw std::invalid_argument("Unknown or malformed argument: " + arg);
    }

    // Check mandatory arguments
    if (mode == Mode::NONE || server.ipVer == IpVersion::None) {
        throw std::invalid_argument("Missing mandatory arguments. Use -h for help.");
    }
}

void Settings::representSettings() const {
    std::cout << "Settings:\n"
              << "  Mode: " << (mode == Mode::TCP ? "TCP" : "UDP") << "\n"
              << "  Server Domain Name: " << server.hostName << "\n"
              << "  Server IP: " << server.ip << "\n"
              << "  Server IP Version: " << (server.ipVer == IpVersion::IPV4 ? "IPv4" : "IPv6") << "\n"
              << "  Server Port: " << server.port << "\n"
              << "  UDP Timeout Confirmation: " << udpTimeoutConfirmation << " ms\n"
              << "  Max UDP Retransmissions: " << maxUdpRetransmissions << "\n";
}

void Settings::printHelp() const {
    std::cout << "Usage: program [options]\n"
              << "Options:\n"
              << "  -t <tcp|udp>       Transport protocol used for connection (mandatory)\n"
              << "  -s <server>        Server IP address or hostname (mandatory)\n"
              << "  -p <port>          Server port (default: 4567)\n"
              << "  -d <timeout>       UDP confirmation timeout in milliseconds (default: 250)\n"
              << "  -r <retries>       Maximum number of UDP retransmissions (default: 3)\n"
              << "  -h                 Prints this help message and exits\n";
}