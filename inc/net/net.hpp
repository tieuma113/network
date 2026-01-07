#pragma once
#include <expected>
#include <system_error>
#include <string>
#include <span>
#include <utility>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
namespace net{
    
inline constexpr int BUFFER_LENGTH = 1024;

template<typename T>
using result = std::expected<T, std::error_code>;

class tcp_socket {
public:
    tcp_socket(std::string IP_address, int port): 
        IP_address(std::move(IP_address)), port(port), fd(0)
    {}

    ~tcp_socket() {
        close(fd);
    } 

    result<void> connect(int timeout){
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0){
            return std::unexpected(std::error_code(errno, std::system_category()));
        }

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, IP_address.c_str(), &addr.sin_addr) != 1){
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        std::cout << "connect successful\n"; 
        return {};
    }

    result<void> send(std::span<std::byte> buffer, int timeout);
    result<std::span<std::byte>> receive(int timeout);


private:
    std::string IP_address;
    int port;
    int fd;
};



}