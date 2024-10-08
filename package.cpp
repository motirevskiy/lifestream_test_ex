#include "package.h"

void Package::fill_package(uint32_t seq_number, uint32_t seq_total, PackageType type, const char* id, const char* data, size_t data_size)
{
    _seq_number = seq_number;
    _seq_total = seq_total;
    _type = type;
    std::memcpy(_id, id, 8);
    std::memcpy(_data, data, data_size);
}

Package::Package() : _seq_number(0), _seq_total(0), _type(PUT)
{
    std::memset(_id, 0, 8);
    std::memset(_data, 0, data_size);
}

uint32_t Package::calculate_checksum(const std::vector<Package>& packages)
{
    uint32_t checksum = 0;

    for (const auto&  package : packages) {
        checksum = crc32c(checksum, (unsigned char *)package.data(), Package::data_size);
    }

    return checksum;
}

uint32_t Package::calculate_checksum(const std::map<uint32_t, Package>& data_map)
{
    uint32_t checksum = 0;

    for (const auto&  [_, package] : data_map) {
        checksum = crc32c(checksum, (unsigned char *)package.data(), Package::data_size);
    }

    return checksum;
}

uint32_t Package::crc32c(uint32_t crc, const unsigned char *buf, size_t len) {
    int k;
    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
    }
    return ~crc;
}