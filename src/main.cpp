#include "net/net.hpp"
#include <memory>
#include <print>
#include <system_error>
#include <unistd.h>

using tcp = net::tcp_socket;

int main() {
     std::println("network library {} ", 1);
     auto sock_tcp = std::make_unique<tcp>("127.0.0.1", 8080);
     auto res = sock_tcp->connect();
     if (!res){
          std::println("connect fail: {}", res.error().message());
     }
     sleep(2);

     return 0;
}
