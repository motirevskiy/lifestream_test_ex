#include "client.h"
#include <cstring>

#include <optional>
#include <random>
#include <algorithm>

Client::Client(const std::string& ip) : _server_ip(ip)
{
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    _server_addr.sin_family = AF_INET;
    _server_addr.sin_port = htons(settings::PORT);
    inet_pton(AF_INET, _server_ip.c_str(), &_server_addr.sin_addr);
}

bool Client::check_attempt(size_t attempt, size_t package_index, std::string filename)
{
    std::cout << "packages[" << package_index << "] " << "of file" << filename << " has not been delivered" << std::endl;

    if (attempt > settings::attempts_count) {
        std::cout << "the server didn't respond" << std::endl;
        return false;
    }

    return true;
}

bool Client::send_file(const std::string& file_path)
{
    const auto& source_packages = create_packages(file_path);
    const auto& shuffled_packages = shuffle_and_duplicate(source_packages);

    const uint32_t local_checksum = Package::calculate_checksum(source_packages);
    size_t attempt = 0;

    size_t index = 0;
    while (true) {
        ++attempt;
        index = index < shuffled_packages.size() - 1 ? index + 1 : 0;

        send_package(shuffled_packages[index]);
        const auto& package = receive_ack();

        if (!package.has_value() || package->type() != ACK || package->seq_number() != shuffled_packages[index].seq_number()) {
            if (!check_attempt(attempt, index, file_path)) {
                break;
            }
            continue;
        }

        if (package->seq_total() == source_packages.size()) {
            return check_final_ack(local_checksum, package.value());
        }
        attempt = 0;
    }

    return false;
}


std::vector<Package> Client::create_packages(const std::string& file_path)
{
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);

    if (!file) {
        std::cerr << "Could not open file: " << file_path << std::endl;;
        return {};
    }

    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    int total_packages = (file_size + settings::data_size - 1) / settings::data_size;
    std::vector<Package> packages(total_packages);
    char file_id[8] = {0};

    for (int i = 0; i < total_packages; ++i) {
        char buffer[settings::data_size];
        memset(buffer, '\0', settings::data_size);
        file.read(buffer, settings::data_size);
        packages[i].fill_package(i, total_packages, PUT, file_id, buffer, file.gcount());
    }

    file.close();

    return packages;
}

std::vector<Package> Client::shuffle_and_duplicate(const std::vector<Package>& packages) {
    if (packages.empty()) {
        return {};
    }

    std::vector<Package> shuffled_packages = packages;
    std::random_device rd;
    std::mt19937 gen(rd());

    std::shuffle(shuffled_packages.begin(), shuffled_packages.end(), gen);

    std::vector<Package> result = shuffled_packages;

    std::uniform_int_distribution<> dist(0, shuffled_packages.size() - 1);
    std::uniform_int_distribution<> dup_count_dist(1, shuffled_packages.size() / 2);

    int duplicate_count = dup_count_dist(gen);
    for (int i = 0; i < duplicate_count; ++i) {
        int index = dist(gen);
        result.push_back(shuffled_packages[index]);
    }

    std::shuffle(result.begin(), result.end(), gen);

    return result;
}

void Client::send_package(const Package& package)
{
    sendto(_sockfd, &package, sizeof(package), 0, (sockaddr*)&_server_addr, sizeof(_server_addr));
}

std::optional<Package> Client::receive_ack()
{
    char ack_buffer[settings::package_size];
    socklen_t addr_len = sizeof(_server_addr);

    pollfd pfd = {.fd = _sockfd, .events = POLLIN};
    if (poll(&pfd, 1, settings::timeout) <= 0) {
        return std::nullopt;
    }

        int bytes_received = recvfrom(_sockfd, ack_buffer, settings::package_size, 0, (sockaddr*)&_server_addr, &addr_len);
        if (bytes_received > 0) {
            Package ack_package;
            std::memcpy(&ack_package, ack_buffer, sizeof(ack_package));
            return ack_package;
        }

    return std::nullopt;
}

bool Client::check_final_ack(const uint32_t& local_checksum, const Package& ack_package)
{
    uint32_t server_checksum;
    std::memcpy(&server_checksum, ack_package.data(), sizeof(server_checksum));

    return local_checksum == server_checksum;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <file_path to filenames file>\n";
        return 1;
    }

    const char* server_ip = argv[1];
    const char* file_path = argv[2];

    Client client(server_ip);
    bool result = client.send_file(file_path);

    if (result) {
        std::cout << "File sent successfully" << std::endl;
    } else {
        std::cout << "Checksums mismatch" << std::endl;
    }

    return result;
}