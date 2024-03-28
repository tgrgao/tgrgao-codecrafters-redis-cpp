#include "Request.h"

#include <iostream>
#include <cctype>

int parse_redis_request(RedisRequest *request, char *request_bytes, int len) {
    std::string request_string(reinterpret_cast<const char*>(request_bytes), len);

    if (request_string.length() < 10) {
        return 10;
    }

    if (request_string[0] != '*') {
        return 11;
    }

    int num_args;
    try {
        num_args = std::stoi(request_string.substr(1, request_string.find("\r\n") - 1));
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return 12;
    }
    catch (const std::out_of_range& e) {
        std::cerr << "Out of range: " << e.what() << std::endl;
        return 13;
    }
    std::string command_string = "";
    bool command_matched = false;
    int at_ind = request_string.find("\r\n") + 2;
    for (int i = 0; i < num_args; ++i) {
        if (request_string[at_ind] != '$') {
            return 4;
        }
        at_ind = request_string.find("\r\n", at_ind) + 2;

        if (!command_matched) {
            command_string += request_string.substr(at_ind, request_string.find("\r\n", at_ind) - at_ind);
            for (auto & c: command_string) c = toupper(c);
            if (command_string == "PING") {
                request->command = RedisRequestCommand::PING;
                command_matched = true;
            } else if (command_string == "ECHO") {
                request->command = RedisRequestCommand::ECHO;
                command_matched = true;
            } else {
                if (i >= 1) {
                    std::cout << "No command matched\n";
                    return 3;
                }
                command_string += " ";
            }
        } else {
            request->arguments.push_back(request_string.substr(at_ind, request_string.find("\r\n", at_ind) - at_ind));
        }
        at_ind = request_string.find("\r\n", at_ind) + 2;
    }

    if (at_ind != len) {
        return 2;
    }

    return 0;
}