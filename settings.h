#ifndef SETTINGS_H_
#define SETTINGS_H_

namespace settings {
    constexpr int package_size = 1472; // maximum package size
    constexpr int header_size = 17;
    constexpr int data_size = package_size - header_size; // package data size

    constexpr int PORT = 12345;

    constexpr int timeout = 1000; // server response timeout
    constexpr int attempts_count = 5; 

} // namespace settings

#endif // SETTINGS_H_