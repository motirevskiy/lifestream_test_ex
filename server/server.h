#ifndef SERVER_H_
#define SERVER_H_

#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <optional>

#include "../package.h"

class Server {
public:
    Server();
    ~Server() { close(_sockfd); }

    void run();

private:
    bool process_package(const Package& package, std::optional<uint32_t>&);

    void send_ack(uint32_t seq_number, uint32_t seq_total, const char* file_id, std::optional<uint32_t> file_checksum = std::nullopt);
    void save_file(const std::map<uint32_t, Package>& packages);

    int _sockfd;
    sockaddr_in _server_addr{};
    sockaddr_in client_addr{};

    std::unordered_map<std::string, std::map<uint32_t, Package>> _files;
    std::unordered_map<std::string, uint32_t> _packages_count;
};

#endif // SERVER_H_