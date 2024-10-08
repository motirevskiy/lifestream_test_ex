#ifndef CLIENT_H_
#define CLIENT_H_

#include <cstdint>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <chrono>
#include <optional>

#include "../package.h"
#include "../settings.h"

class Client {
public:
    Client(const std::string& ip);
    ~Client() { close(_sockfd); }

    bool send_file(const std::string& file_path);

private:
    std::vector<Package> create_packages(const std::string& file_path);
    std::vector<Package> shuffle_and_duplicate(const std::vector<Package>& packages);
    void send_package(const Package& package);
    std::optional<Package> receive_ack();
    
    bool check_final_ack(const uint32_t& local_checksum, const Package& ack_package);
    bool check_attempt(size_t attempt, size_t package_index, std::string filename);

    std::string _server_ip;
    int _sockfd;
    sockaddr_in _server_addr{};

};

#endif // CLIENT_H_