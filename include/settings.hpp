/**
 * @file settings.cpp
 * @brief Implementation of the Settings class
 * @author Martin Mendl <x247581>
 * @date 18.4.2025
*/

#ifndef SETTINGS_HPPP
#define SETTINGS_HPPP

#include <string>
#include <cstdint>
#include "utils.hpp"

/// struct for network address
struct NetworkAdress {
    std::string hostName;
    std::string ip;
    IpVersion ipVer;
    uint16_t port;
};

/**
 * @brief Function to determine the target type (IP address or domain name).
 * @param target The target string to analyze.
 * @return The determined target type (IP_v4, IP_v6, DOMAIN_NAME, or UNKNOWN).
*/
TargetType determinTargetType(const std::string &target);

/**
 * @class Settings
 * @brief Class for handling command-line arguments and settings.
 *
 * This class parses command-line arguments, validates them,
 * and provides access to the parsed settings.
 */
class Settings {

    public:
        /**
         * @brief Constructor that initializes settings based on command-line arguments.
         * @param argc The number of command-line arguments.
         * @param argv The array of command-line arguments.
         */
        Settings(int argc, char* argv[]);

        /**
         * @brief Destructor.
         */
        ~Settings() {};

        /**
         * @brief Gets the mode of operation (TCP or UDP).
         * @return The mode of operation.
         */
        Mode getMode() const { return mode; };

        /**
         * @brief Gets the server address.
         * @return The server address.
         */
        NetworkAdress getServer() const { return server; };

        /**
         * @brief Gets the UDP timeout confirmation value.
         * @return The UDP timeout confirmation value.
         */
        int getUdpTimeoutConfirmation() const { return udpTimeoutConfirmation; };

        /**
         * @brief Gets the maximum number of UDP retransmissions.
         * @return The maximum number of UDP retransmissions.
         */
        int getMaxUdpRetransmissions() const { return maxUdpRetransmissions; };

        /**
         * @brief Prints the settings to the console.
         *
         * This method provides a summary of the current settings.
         */
        void representSettings() const;

    private:
    
        /**
         * @brief Parses command-line arguments and initializes settings.
         * @param argc The number of command-line arguments.
         * @param argv The array of command-line arguments.
         */
        void printHelp() const;

        /**
         * @brief Converts a domain name to an IP address.
         * @param domain The domain name to convert.
         * @return The corresponding IP address as a string.
        */
        std::string getIpFromDomain(const std::string& domain);
        
        Mode mode;                          ///< Mode of operation (TCP or UDP).
        NetworkAdress server;               ///< Server address.
        int udpTimeoutConfirmation;         ///< Timeout for UDP confirmation in milliseconds.
        int maxUdpRetransmissions;          ///< Maximum number of UDP retransmissions.
};

#endif // SETTINGS_HPPP