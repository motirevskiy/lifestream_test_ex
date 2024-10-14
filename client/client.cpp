#include "client.h"

#include <cstdint>
#include <mutex>
#include <random>
#include <algorithm>
#include <poll.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include <netdb.h>
#include <unordered_map>

Client::Client() : _server_ip(get_local_ip())
{
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    _server_addr.sin_family = AF_INET;
    _server_addr.sin_port = htons(settings::PORT);
    inet_pton(AF_INET, _server_ip.c_str(), &_server_addr.sin_addr);
}

std::string Client::get_local_ip() {
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;

    if (gethostname(hostbuffer, sizeof(hostbuffer)) == -1) {
        return "";
    }

    host_entry = gethostbyname(hostbuffer);
    if (!host_entry) {
        return "";
    }

    IPbuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));

    return std::string(IPbuffer);
}

void Client::log(const std::string& message)
{
    std::lock_guard<std::mutex> lock(_log_mutex);
    std::cout << message <<std::endl;
}

bool Client::check_attempt(size_t attempt, size_t package_index, std::string filename)
{
    log(std::string("package[") + std::to_string(package_index) + "] of file " + std::string(filename) + std::string(" has not been delivered"));

    if (attempt > settings::attempts_count) {
        log("the server didn't respond");
        return false;
    }

    return true;
}

bool Client::send_file(const std::string& file_path)
{
    const auto& source_packages = create_packages(file_path);
    if (source_packages.empty()) {
        return false;
    }

    const auto& shuffled_packages = shuffle_and_duplicate(source_packages);
    const uint32_t local_checksum = Package::calculate_checksum(source_packages);

    size_t attempt = 0;
    size_t index = 0;

    while (true) {
        ++attempt;
        index = index < shuffled_packages.size() - 1 ? index + 1 : 0;

        const auto& to_send = shuffled_packages[index];
        send_package(to_send);

        const auto& package = receive_ack();

        if (!package.has_value() || package->type() != ACK || package->seq_number() != to_send.seq_number()) {
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

std::map<std::string, bool> Client::send_files_multithread(const std::vector<std::string>& filenames)
{
    std::vector<std::thread> send_files_threads;
    std::map<std::string, bool> result;

    for (const auto& filename : filenames) {
        const auto& send_file_action = [&result, filename, this]() {
            result[filename] = send_file(filename);
        };
    
        std::thread send_file_thread(send_file_action);
        send_files_threads.push_back(std::move(send_file_thread));
    }

    for (auto& thread : send_files_threads) {
        thread.join();
    }

    return result;
}

std::map<std::string, bool> Client::send_files_singlethread(const std::vector<std::string>& filenames)
{
    std::map<std::string, bool> result;

    for (const auto& filename : filenames) {
        result[filename] = send_file(filename);
    }

    return result;
}

bool Client::send_multiple_files(const std::vector<std::string>& filenames)
{
    std::vector<std::vector<Package>> files_packages;
    std::vector<Package> shuffled_files_packages;

    for (const auto& filename : filenames) {
        files_packages.push_back(create_packages(filename));
    }

    std::unordered_map<std::string, uint32_t> id_to_checksums;
    id_to_checksums.reserve(files_packages.size());

    for (const auto& packages : files_packages) {
        id_to_checksums[packages.front().id()] = Package::calculate_checksum(packages);
        std::copy(packages.begin(), packages.end(), std::back_inserter(shuffled_files_packages));
    }

    shuffled_files_packages = shuffle_and_duplicate(shuffled_files_packages);

    size_t attempt = 0;
    size_t index = 0;

    while(!id_to_checksums.empty()) {
        ++attempt;
        index = index < shuffled_files_packages.size() - 1 ? index + 1 : 0;

        const auto& to_send = shuffled_files_packages[index];

        send_package(to_send);
        const auto& package = receive_ack();

        if (!package.has_value() || package->type() != ACK || package->seq_number() != to_send.seq_number()) {
            if (!check_attempt(attempt, index, package->id())) {
                break;
            }
            continue;
        }

        if (package->seq_total() == to_send.seq_total()) {
            bool result = check_final_ack(id_to_checksums[package->id()], package.value());
            if (result) {
                log("File has been delivered");
            } else {
                log("File has not been delivered");
            }
            id_to_checksums.erase(package->id());
        }

        attempt = 0;
    }

    return true;
}


std::vector<Package> Client::create_packages(const std::string& file_path)
{
    int id = std::rand() % std::numeric_limits<int>::max();

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
    memset(&file_id, 0, sizeof(file_id));
    memcpy(file_id, &id, sizeof(int));

    for (int i = 0; i < total_packages; ++i) {
        char buffer[settings::data_size] = {0};
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
    std::lock_guard<std::mutex> lock(_send_mutex);
    sendto(_sockfd, &package, sizeof(package), 0, (sockaddr*)&_server_addr, sizeof(_server_addr));
}

std::optional<Package> Client::receive_ack()
{
    char ack_buffer[settings::package_size];
    socklen_t addr_len = sizeof(_server_addr);

    std::unique_lock<std::mutex> lock(_receive_mutex);
    int bytes_received = recvfrom(_sockfd, ack_buffer, settings::package_size, 0, (sockaddr*)&_server_addr, &addr_len);
    lock.unlock();
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

std::vector<std::string> get_filenames(const std::string& filename) {
    std::vector<std::string> filenames;
    std::ifstream file_list(filename);
    
    if (!file_list) {
        std::cerr << "Could not open file list: " << filename << std::endl;
        return filenames;
    }

    std::string file;
    while (std::getline(file_list, file)) {
        if (!file.empty()) {
            filenames.push_back(file);
        }
    }

    file_list.close();
    return filenames;
}

int main(int argc, char *argv[]) {
    bool multithread = false;

    if (argc == 3 && std::string(argv[2]) == "-m") {
        multithread = true;
    } else if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << "<file_path to filenames file> [-m]\n";
        return 1;
    }

    const auto& filenames = get_filenames(argv[1]);
    if (filenames.empty()) {
        return 0;
    }

    Client client{};

    std::map<std::string, bool> result;
    
    if (multithread) {
        result = client.send_files_multithread(filenames);
    } else {
        result = client.send_files_singlethread(filenames);
    }

    for (const auto& [filename, send_result] : result) {
        if (!send_result) {
            std::cerr << "Could not send file: " << filename << std::endl;
        } else {
            std::cout << "File " << filename << " successfully sent" << std::endl;
        }
    }

    return 0;
}