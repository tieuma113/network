#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <system_error>
#include <vector>
// #include <sys/socket.h>
// #include <arpa/inet.h>
// #include <unistd.h>
namespace net {

inline constexpr int BUFFER_LENGTH = 1024;

template <typename T> using result = std::expected<T, std::error_code>;

class tcp_socket {
  public:
    tcp_socket(std::string IP_address, uint16_t port);

    ~tcp_socket();

    result<void> connect();

    [[nodiscard]] result<void> send(const std::vector<std::byte>& buffer) const;
    std::vector<std::byte> receive();

  private:
    std::string m_IP_address;
    uint16_t m_port;
    int m_fd;
};

} // namespace net