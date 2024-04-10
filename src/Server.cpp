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
#include <cstring>
#include <cerrno>

#include "Cache.h"
#include "API.h"
#include "resp-parser/Protocol.h"

struct ServerInfo {
    std::string host;
    int port;
};

ServerInfo server_info;

int replica_handshake(int replication_client_socket, Cache& cache) {
    char recv_buffer[2048];

    struct sockaddr_in master_addr;
    master_addr.sin_family = AF_INET;
    if (cache.get_replica_of_host() == "localhost") {
        master_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    } else {
        master_addr.sin_addr.s_addr = inet_addr(cache.get_replica_of_host().c_str());
    }
    master_addr.sin_port = htons(cache.get_replica_of_port());

    // Connect to the server
    if (connect(replication_client_socket, (struct sockaddr *)&master_addr, sizeof(master_addr)) == -1) {
        std::cerr << "Failed to connect to master server at " << cache.get_replica_of_host() << ":" << cache.get_replica_of_port() << ". " << strerror(errno) << std::endl;
        return -1;
    }

    // Send data to the server
    std::string message = "*1\r\n$4\r\nping\r\n";
    if (send(replication_client_socket, message.c_str(), message.size(), 0) == -1) {
        std::cerr << "Failed to send data to master server" << std::endl;
        return -1;
    }

    int bytes_recv = recv(replication_client_socket, recv_buffer, 2048, 0);
    if (bytes_recv == 0) {
        std::cout << "Client disconnected\n";
        return -1;
    }

    if (bytes_recv < 0) {
        std::cout << "Error receiving data\n";
        return -1;
    }

    if (std::string(recv_buffer, bytes_recv) != "+PONG\r\n") {
        std::cout << "Response was not OK\n";
        return -1;
    }

    message = "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$" + std::to_string(std::to_string(server_info.port).length()) + "\r\n" + std::to_string(server_info.port) + "\r\n";
    if (send(replication_client_socket, message.c_str(), message.size(), 0) == -1) {
        std::cerr << "Failed to send data to master server" << std::endl;
        return -1;
    }

    bytes_recv = recv(replication_client_socket, recv_buffer, 2048, 0);
    if (bytes_recv == 0) {
        std::cout << "Client disconnected\n";
        return -1;
    }

    if (bytes_recv < 0) {
        std::cout << "Error receiving data\n";
        return -1;
    }

    if (std::string(recv_buffer, bytes_recv) != "+OK\r\n") {
        std::cout << "Response was not OK\n";
        return -1;
    }

    message = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
    if (send(replication_client_socket, message.c_str(), message.size(), 0) == -1) {
        std::cerr << "Failed to send data to master server" << std::endl;
        return -1;
    }

    bytes_recv = recv(replication_client_socket, recv_buffer, 2048, 0);
    if (bytes_recv == 0) {
        std::cout << "Client disconnected\n";
        return -1;
    }

    if (bytes_recv < 0) {
        std::cout << "Error receiving data\n";
        return -1;
    }

    if (std::string(recv_buffer, bytes_recv) != "+OK\r\n") {
        std::cout << "Response was not OK\n";
        return -1;
    }

    message = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";
    if (send(replication_client_socket, message.c_str(), message.size(), 0) == -1) {
        std::cerr << "Failed to send data to master server" << std::endl;
        return -1;
    }

    return 0;
}

void handle_replication(int replication_client_socket, Cache& cache) {
    if (cache.get_role() == "master") {
        while (true) {
            std::string message = cache.get_next_propagation();
            // std::cout << "MASTER: Sending replication propagation " + message + "\n";
            for (int conn : cache.get_replica_conns()) {
                if (send(conn, message.c_str(), message.size(), 0) == -1) {
                    std::cerr << "Failed to send data to replica server" << std::endl;
                }
            }
            // std::cout << "Replication propagation successfully sent!\n";
        }
    } else {
        std::string read_buf_str;
        char read_buf[2048];

        RedisExpression fullpsync_expression;

        while (fullpsync_expression.finished != true) {
            fullpsync_expression.consume_input(read_buf_str);
            
            int bytes_recv = recv(replication_client_socket, read_buf, 2048, 0);
            if (bytes_recv == 0) {
                std::cout << "Client disconnected\n";
                break;
            }
            if (bytes_recv < 0) {
                std::cout << "Error receiving data\n";
                continue;
            }
            read_buf_str += std::string(read_buf, bytes_recv);
        }

        if (fullpsync_expression.finished != true) {
            return;
        }

        RedisExpression rdb_file_expression;

        while (rdb_file_expression.finished != true) {
            rdb_file_expression.consume_input(read_buf_str);
            
            int bytes_recv = recv(replication_client_socket, read_buf, 2048, 0);
            if (bytes_recv == 0) {
                std::cout << "Client disconnected\n";
                break;
            }
            if (bytes_recv < 0) {
                std::cout << "Error receiving data\n";
                continue;
            }
            read_buf_str += std::string(read_buf, bytes_recv);
        }

        if (rdb_file_expression.finished != true) {
            return;
        }

        std::cout << "RDB file received: " << rdb_file_expression.get_string_value() << std::endl;

        while (true)
        {   
            RedisExpression expression;
            expression.consume_input(read_buf_str);

            while (expression.finished != true) {
                // std::cout << "HERE: " << read_buf_str << std::endl;
                
                int bytes_recv = recv(replication_client_socket, read_buf, 2048, 0);
                if (bytes_recv == 0) {
                    std::cout << "Client disconnected\n";
                    break;
                }
                if (bytes_recv < 0) {
                    std::cout << "Error receiving data\n";
                    continue;
                }
                read_buf_str += std::string(read_buf, bytes_recv);
                expression.consume_input(read_buf_str);
            }

            if (expression.finished != true) {
                break;
            }

            // std::cout << "Replica received from master: " << expression.to_string() << std::endl;

            RedisRequest request;

            int ret = parse_redis_request(request, expression);
            if (ret != 0)
            {
                // sprintf(send_buf, "Error parsing request", ret);
                // send(client_fd, send_buf, 50, 0);
                std::cout << "Error parsing request\n";
                continue;
            }

            std::string response_string = handle_request(request, cache, replication_client_socket); // do not send response
        }
    }

    return;
}

void handle_client(int client_fd, Cache &cache) {
    std::string read_buf_str;
    char read_buf[2048];
    char send_buf[2048];
    while (true)
    {
        RedisExpression expression;
        expression.consume_input(read_buf_str);

        while (expression.finished != true) {            
            int bytes_recv = recv(client_fd, read_buf, 2048, 0);
            if (bytes_recv == 0) {
                std::cout << "Client disconnected\n";
                break;
            }
            if (bytes_recv < 0) {
                std::cout << "Error receiving data\n";
                continue;
            }
            read_buf_str += std::string(read_buf, bytes_recv);
            expression.consume_input(read_buf_str);
        }

        if (expression.finished != true) {
            break;
        }

        // std::cout << cache.get_role() <<  " received: " << expression.to_string() << std::endl;

        RedisRequest request;

        int ret = parse_redis_request(request, expression);
        if (ret != 0)
        {
            // sprintf(send_buf, "Error parsing request", ret);
            // send(client_fd, send_buf, 50, 0);
            std::cout << "Error parsing request\n";
            continue;
        }

        std::string response_string = handle_request(request, cache, client_fd);
        if (response_string.length() > 0)
        {
            send(client_fd, response_string.c_str(), response_string.length(), 0);
        }
    }

    close(client_fd);

    return;
}

int main(int argc, char *argv[])
{
    Cache cache;
    cache.make_master();

    server_info.host = "localhost";
    server_info.port = 6379;

    int i = 1;
    while (i < argc)
    {
        if (std::string(argv[i]) == "--port")
        {
            try
            {
                server_info.port = std::stoi(argv[i + 1]);
            }
            catch (const std::invalid_argument &e)
            {
                std::cerr << "Invalid argument: " << e.what() << std::endl;
                throw e;
            }
            catch (const std::out_of_range &e)
            {
                std::cerr << "Out of range: " << e.what() << std::endl;
                throw e;
            }
            i += 2;
            continue;
        }

        if (std::string(argv[i]) == "--replicaof")
        {
            int replica_of_port;
            try
            {
                replica_of_port = std::stoi(argv[i + 2]);
            }
            catch (const std::invalid_argument &e)
            {
                std::cerr << "Invalid argument: " << e.what() << std::endl;
                throw e;
            }
            catch (const std::out_of_range &e)
            {
                std::cerr << "Out of range: " << e.what() << std::endl;
                throw e;
            }
            cache.make_replica(argv[i + 1], replica_of_port);
            i += 3;
            continue;
        }

        ++i;
    }

    int replication_client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cache.get_role() == "slave") {
        if (replica_handshake(replication_client_socket, cache) != 0) {
            return 1;
        }
    }
    std::thread replication_thread = std::thread(handle_replication, replication_client_socket, std::ref(cache));

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_info.port);

    std::cout << "Listening on port " << server_info.port << "\n";

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        std::cerr << "Failed to bind to port " << server_info.port << "\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_socket, connection_backlog) != 0)
    {
        std::cerr << "listen failed\n";
        return 1;
    }

    std::cout << "Waiting for a client to connect...\n";

    std::vector<std::thread> client_threads;

    while (true)
    {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);

        int client_fd = accept(server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);

        std::cout << "Client connected\n";

        client_threads.push_back(std::thread(handle_client, client_fd, std::ref(cache)));
    }

    for (std::thread &client_thread : client_threads)
    {
        if (client_thread.joinable())
        {
            client_thread.join();
        }
    }

    replication_thread.join();

    close(server_socket);
    close(replication_client_socket);

    return 0;
}
