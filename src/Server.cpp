#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <vector>
#include <map>
#include <functional>

#include "Cache.h"
#include "Request.h"

void conn_thread(int client_fd, Cache& cache) {
  while (true) {
    char read_buf[2048];
    char send_buf[2048];

    RedisRequest request;

    int bytes_recv = recv(client_fd, read_buf, 2048, 0);

    if (bytes_recv == 0) {
      std::cout << "Client disconnected\n";
      break;
    }

    if (bytes_recv < 0) {
      std::cout << "Error receiving data\n";
      continue;
    }
    int ret = parse_redis_request(&request, read_buf, bytes_recv);
    if (ret != 0) {
      // sprintf(send_buf, "%d\n", ret);
      // send(client_fd, send_buf, 5, 0);
      std::cout << "Error parsing data\n";
      continue;
    }

    if (request.command == RedisRequestCommand::PING) {
      send(client_fd, "+PONG\r\n", 7, 0);
    } else if (request.command == RedisRequestCommand::ECHO) {
      std::string response_string = "+" + request.arguments[0] + "\r\n";
      send(client_fd, response_string.c_str(), response_string.length(), 0);
    } else if (request.command == RedisRequestCommand::SET) {
      cache.mut.lock();
      cache.map[request.arguments[0]] = request.arguments[1];
      cache.mut.unlock();
      std::string response_string = "+OK\r\n";
      send(client_fd, response_string.c_str(), response_string.length(), 0);
    } else if (request.command == RedisRequestCommand::GET) {
      std::string response_string;
      cache.mut.lock();
      if (cache.map.find(request.arguments[0]) != cache.map.end()) {
        response_string = "+" + cache.map[request.arguments[0]] + "\r\n";
      } else {
        response_string = "$-1\r\n";
      }
      cache.mut.unlock();
      send(client_fd, response_string.c_str(), response_string.length(), 0);
    }
  }

  close(client_fd);

  return;
}

int main(int argc, char **argv) {
  Cache cache;

  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  std::cout << "Waiting for a client to connect...\n";

  std::vector<std::thread> client_threads;

  while (true) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);

    std::cout << "Client connected\n";

    client_threads.push_back(std::thread(conn_thread, client_fd, std::ref(cache)));
  }

  for (std::thread& client_thread : client_threads) {
    if (client_thread.joinable()) {
      client_thread.join();
    }
  }
  
  close(server_fd);

  return 0;
}
