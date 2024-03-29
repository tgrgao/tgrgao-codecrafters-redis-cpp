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
#include <chrono>
#include <ctime>

#include "Cache.h"
#include "API.h"


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
    int ret = parse_redis_request(request, read_buf, bytes_recv);
    if (ret != 0) {
      // sprintf(send_buf, "Error parsing request", ret);
      // send(client_fd, send_buf, 50, 0);
      std::cout << "Error parsing request\n";
      continue;
    }

    std::string response_string = handle_request(request, cache);
    if (response_string.length() > 0) {
      send(client_fd, response_string.c_str(), response_string.length(), 0);
    }
  }

  close(client_fd);

  return;
}

int main(int argc, char *argv[]) {
  Cache cache;
  cache.make_master();

  int port = 6379;
  int i = 1;
  while (i < argc) {
    if (std::string(argv[i]) == "--port") {
      try {
        port = std::stoi(argv[i+1]);
      } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        throw e;
      } catch (const std::out_of_range& e) {
        std::cerr << "Out of range: " << e.what() << std::endl;
        throw e;
      }
      i += 2;
      continue;
    }

    if (std::string(argv[i]) == "--replicaof") {
      int replica_of_port;
      try {
        replica_of_port = std::stoi(argv[i+2]);
      } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        throw e;
      } catch (const std::out_of_range& e) {
        std::cerr << "Out of range: " << e.what() << std::endl;
        throw e;
      }
      cache.make_replica(argv[i+1], replica_of_port);
      i += 3;
      continue;
    }

    ++i;
  }
  
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
  server_addr.sin_port = htons(port);

  std::cout << "Listening on port " << port << "\n";
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port " << port << "\n";
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
