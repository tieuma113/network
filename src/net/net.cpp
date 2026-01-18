#include "net/net.hpp"
#include <cerrno>
#include <expected>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <system_error>
#include <unistd.h>

namespace net {
    tcp_socket::tcp_socket(std::string IP_address, uint16_t port): 
        m_IP_address(std::move(IP_address)), m_port(port), m_fd(0)
    {}

    tcp_socket::~tcp_socket() {
        close(m_fd);
    }

    result<void> tcp_socket::connect(){
        m_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_fd < 0){
            return std::unexpected(std::error_code(errno, std::system_category()));
        }

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(m_port);
        if (inet_pton(AF_INET, m_IP_address.c_str(), &addr.sin_addr) != 1){
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1){
            return std::unexpected(std::error_code(errno, std::system_category()));
        }

        std::cout << "connect successful\n"; 
        return {};
    }

    result<void> tcp_socket::send(const std::vector<std::byte> &buffer) const {
        if (m_fd <= 0){
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        if (::send(m_fd, buffer.data(), buffer.size() - 1, 0) < 0){
            return std::unexpected(std::error_code(errno, std::system_category()));
        }
        return {};
    }
    
    std::vector<std::byte> tcp_socket::receive(){
        return {};
    }
}
