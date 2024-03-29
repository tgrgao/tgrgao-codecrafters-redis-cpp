#include "API.h"

#include <iostream>
#include <cctype>

int parse_redis_request(RedisRequest& request, char *request_bytes, int len) {
    std::string request_string(reinterpret_cast<const char*>(request_bytes), len);

    if (request_string.length() < 10) {
        return -1;
    }

    if (request_string[0] != '*') {
        return -1;
    }

    int num_args;
    try {
        num_args = std::stoi(request_string.substr(1, request_string.find("\r\n") - 1));
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return -1;
    }
    catch (const std::out_of_range& e) {
        std::cerr << "Out of range: " << e.what() << std::endl;
        return -1;
    }
    std::string command_string = "";
    bool command_matched = false;
    int at_ind = request_string.find("\r\n") + 2;

    for (int i = 0; i < num_args; ++i) {
        if (request_string[at_ind] != '$') {
            return -1;
        }
        at_ind = request_string.find("\r\n", at_ind) + 2;

        if (!command_matched) {
            command_string += request_string.substr(at_ind, request_string.find("\r\n", at_ind) - at_ind);
            for (auto & c: command_string) c = toupper(c);
            if (command_string == "PING") {
                request.command = RedisRequestCommand::PING;
                command_matched = true;
            } else if (command_string == "ECHO") {
                request.command = RedisRequestCommand::ECHO;
                command_matched = true;
            } else if (command_string == "SET") {
                request.command = RedisRequestCommand::SET;
                command_matched = true;
            } else if (command_string == "GET") {
                request.command = RedisRequestCommand::GET;
                command_matched = true;
            } else if (command_string == "INFO") {
                request.command = RedisRequestCommand::INFO;
                command_matched = true;
            } else {
                if (i >= 1) {
                    std::cout << "No command matched\n";
                    return -1;
                }
                command_string += " ";
            }
        } else {
            request.arguments.push_back(request_string.substr(at_ind, request_string.find("\r\n", at_ind) - at_ind));
            if ((request.command == RedisRequestCommand::SET && i == 3) || (request.command == RedisRequestCommand::INFO)) {
                for (auto & c: request.arguments.back()) c = tolower(c);
            }
        }
        at_ind = request_string.find("\r\n", at_ind) + 2;
    }

    if (at_ind != len) {
        return -1;
    }

    return 0;
}

std::string handle_info_request(RedisRequest& request, Cache& cache) {
    std::vector<std::string> info_args = {"replication"};
    std::string ret;
    if (request.arguments.size() != 0) {
        info_args = request.arguments;
    }

    for (std::string info_arg : info_args) {
        if (info_arg == "replication") {
            ret += "role:" + cache.get_role() + "\n";
        }
    }

    return format_bulk_string(ret);
}

std::string handle_set_request(RedisRequest& request, Cache& cache) {
    std::chrono::milliseconds::rep expiry_time;
    if (request.arguments.size() >= 4 && (request.arguments[2] == "px")) { // crude but probably still most concise way of checking case-insensitive
        int expiry_milliseconds;
        try {
            expiry_milliseconds = std::stoi(request.arguments[3]);
        }
        catch (const std::invalid_argument& e) {
            std::cerr << "Invalid argument: " << e.what() << std::endl;
            return "";
        }
        catch (const std::out_of_range& e) {
            std::cerr << "Out of range: " << e.what() << std::endl;
            return "";
        }
        expiry_time = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + std::chrono::milliseconds(expiry_milliseconds)).time_since_epoch()).count();
    } else {
        expiry_time = -1;
    }

    if (cache.set(request.arguments[0], request.arguments[1], expiry_time) != 0) {
        return "";
    }
    
    return "+OK\r\n";
}

std::string handle_get_request(RedisRequest& request, Cache& cache) {
    std::string value;
    if (cache.get(request.arguments[0], value) != 0) {
        return "$-1\r\n";
    }
    return format_bulk_string(value);
}

std::string handle_request(RedisRequest& request, Cache& cache) {
    if (request.command == RedisRequestCommand::PING) {
        return "+PONG\r\n";
    } else if (request.command == RedisRequestCommand::ECHO) {
        return "+" + request.arguments[0] + "\r\n";
    } else if (request.command == RedisRequestCommand::SET) {
        return handle_set_request(request, cache);
    } else if (request.command == RedisRequestCommand::GET) {
        return handle_get_request(request, cache);
    } else if (request.command == RedisRequestCommand::INFO) {
        return handle_info_request(request, cache);
    }
    return "";
}

std::string format_bulk_string(std::string s) {
    return "$" + std::to_string(s.length()) + "\r\n" + s + "\r\n";
}