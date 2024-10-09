#include "server.h"
#include "../settings.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <map>
#include <poll.h>

Server::Server()
{
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    _server_addr.sin_family = AF_INET;
    _server_addr.sin_port = htons(settings::PORT);
    _server_addr.sin_addr.s_addr = INADDR_ANY;
    _error = bind(_sockfd, (sockaddr*)&_server_addr, sizeof(_server_addr));
}

bool Server::process_package(const Package& package, std::optional<uint32_t>& checksum)
{
    if (package.type() != PUT) {
        return false;
    }

    auto& file_packages = _files[package.id()];
    

    if (file_packages.count(package.seq_number()) == 0) {
        file_packages.emplace(package.seq_number(), package);
    }

    if (file_packages.size() != package.seq_total()) {
        return true;
    }

    checksum = Package::calculate_checksum(file_packages);
    return true;

}

void Server::run()
{
    socklen_t addr_len = sizeof(client_addr);
    char buffer[settings::package_size];

    while (true) {

        pollfd pfd = {.fd = _sockfd, .events = POLLIN, .revents = POLLNVAL };
        if (poll(&pfd, 1, settings::timeout) <= 0) { // to avoid blocking
            continue;
        }

        int bytes_received = recvfrom(_sockfd, buffer, settings::package_size, 0, (sockaddr*)&client_addr, &addr_len);
        if (bytes_received < 0) continue;

        Package package;
        std::memcpy(&package, buffer, sizeof(package));

        std::optional<uint32_t> file_checksum;
        if (!process_package(package, file_checksum)) {
            continue;
        }

        send_ack(package.seq_number(), _files[package.id()].size(), package.id(), file_checksum);
        if (file_checksum.has_value()) {
            save_file(_files[package.id()]);
            _packages_count.erase(package.id());
            _files.erase(package.id());
        }

    }
}

void Server::send_ack(uint32_t seq_number, uint32_t seq_total, const char* file_id, std::optional<uint32_t> file_checksum)
{
    Package ack;
    const auto& data = file_checksum.has_value() ? reinterpret_cast<const char*>(&file_checksum) : "";

    ack.fill_package(seq_number, seq_total, ACK, file_id, data, sizeof(data));
    sendto(_sockfd, &ack, sizeof(ack), 0, (sockaddr*)&client_addr, sizeof(client_addr));
}

void Server::save_file(const std::map<uint32_t, Package>& packages)
{
    static size_t index = 0;
    const std::string file_name = "file_" + std::to_string(++index);

    std::ofstream out_file(std::data(file_name), std::ios::binary);
    for (const auto& [_, package] : packages) {
        out_file.write(package.data(), settings::data_size);
    }
    out_file.close();
    std::cout << "File saved as " << file_name << "\n";
}

int main()
{
    Server server{};
    if (!server.is_valid()) {
        return 1;
    }

    server.run();

    return 0;
}