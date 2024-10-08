#ifndef PACKAGE_H_
#define PACKAGE_H_

#include <cstring>
#include <cstdint>
#include <vector>
#include <map>

enum PackageType {
    ACK = 0,
    PUT = 1
};

struct Package {
public:
    static constexpr int package_size = 1472;
    static constexpr int header_size = 17;
    static constexpr int data_size = package_size - header_size;
    static uint32_t calculate_checksum(const std::vector<Package>& data_vector);
    static uint32_t calculate_checksum(const std::map<uint32_t, Package>& data_map);

    Package();
    void fill_package(uint32_t seq_number, uint32_t seq_total, PackageType type, const char* id, const char* data, size_t data_size);

    uint32_t seq_number() const { return _seq_number; }
    uint32_t seq_total() const { return _seq_total; }
    uint8_t type() const { return _type; }
    const char* id() const { return _id; }
    const char* data() const { return _data; }

private:
    uint32_t _seq_number;
    uint32_t _seq_total;
    uint8_t _type;
    char _id[8];
    char _data[data_size];

    static uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len);
};

#endif // PACKAGE_H_