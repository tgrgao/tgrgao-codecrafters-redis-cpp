#include "API.h"

#include <iostream>
#include <cctype>

int parse_redis_request(RedisRequest& request, RedisExpression& expression) {
    if (expression.finished != true || expression.type != RedisExpression::RedisType::Array || expression.length < 1) {
        return -1;
    }

    std::string command_string = expression.array[0]->get_string_value();
    for (auto & c: command_string) {
        c = toupper(c);
    }

    int arguments_start = 1;
        
    if (command_string == "PING") {
        request.command = RedisRequestCommand::PING;
    } else if (command_string == "ECHO") {
        request.command = RedisRequestCommand::ECHO;
    } else if (command_string == "SET") {
        request.command = RedisRequestCommand::SET;
    } else if (command_string == "GET") {
        request.command = RedisRequestCommand::GET;
    } else if (command_string == "INFO") {
        request.command = RedisRequestCommand::INFO;
    } else if (command_string == "REPLCONF") {
        request.command = RedisRequestCommand::REPLCONF;
    } else if (command_string == "PSYNC") {
        request.command = RedisRequestCommand::PSYNC;
    } else {
        std::cout << "No command matched\n";
        return -1;
    }
    
    for (int i = arguments_start; i < expression.length; ++i) {
        request.arguments.push_back(std::move(expression.array[i]));
    }

    return 0;
}

std::string handle_info_request(RedisRequest& request, Cache& cache) {
    std::vector<std::string> info_args = {"replication"};
    std::string ret;
    if (request.arguments.size() != 0) {
        info_args = {};
        for (int i = 0; i < request.arguments.size(); ++i) {
            info_args.push_back(request.arguments[i]->get_string_value());
        }
    }

    for (std::string info_arg : info_args) {
        if (info_arg == "replication") {
            ret += "role:" + cache.get_role() + "\n";
            if (cache.get_role() == "master") {
                ret += "master_replid:" + cache.get_master_replid() + "\n";
                ret += "master_repl_offset:" + std::to_string(cache.get_master_repl_offset()) + "\n";
            }
        }
    }

    return format_bulk_string(ret);
}

std::string handle_set_request(RedisRequest& request, Cache& cache) {
    std::chrono::milliseconds::rep expiry_time;
    if (request.arguments.size() >= 4 && (request.arguments[2]->get_string_value() == "px")) { // crude but probably still most concise way of checking case-insensitive
        int expiry_milliseconds = std::stoi(request.arguments[3]->get_string_value());
        expiry_time = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + std::chrono::milliseconds(expiry_milliseconds)).time_since_epoch()).count();
    } else {
        expiry_time = -1;
    }

    if (cache.set(request.arguments[0]->get_string_value(), request.arguments[1]->get_string_value(), expiry_time) != 0) {
        return "";
    }
    
    return "+OK\r\n";
}

std::string handle_get_request(RedisRequest& request, Cache& cache) {
    std::string value;
    if (cache.get(request.arguments[0]->get_string_value(), value) != 0) {
        return "$-1\r\n";
    }
    return format_bulk_string(value);
}

std::string handle_psync_request(RedisRequest& request, Cache& cache, int conn) {
    std::string ret;

    if (request.arguments[0]->get_string_value() == "?" && request.arguments[1]->get_string_value() == "-1") {
        ret = "+FULLRESYNC " + cache.get_master_replid() + " " + std::to_string(cache.get_master_repl_offset()) + "\r\n";

        std::string empty_rdb_binary = "\x52\x45\x44\x49\x53\x30\x30\x31\x31\xfa\x09\x72\x65\x64\x69\x73\x2d\x76\x65\x72\x05\x37\x2e\x32\x2e\x30\xfa\x0a\x72\x65\x64\x69\x73\x2d\x62\x69\x74\x73\xc0\x40\xfa\x05\x63\x74\x69\x6d\x65\xc2\x6d\x08\xbc\x65\xfa\x08\x75\x73\x65\x64\x2d\x6d\x65\x6d\xc2\xb0\xc4\x10\x00\xfa\x08\x61\x6f\x66\x2d\x62\x61\x73\x65\xc0\x00\xff\xf0\x6e\x3b\xfe\xc0\xff\x5a\xa2";

        ret += "$" + std::to_string(empty_rdb_binary.length()) + "\r\n" + empty_rdb_binary;
    }

    cache.add_replica_conn(conn);

    return ret;
}

std::string handle_request(RedisRequest& request, Cache& cache, int conn) {
    if (request.command == RedisRequestCommand::PING) {
        return format_simple_string("PONG");
    } else if (request.command == RedisRequestCommand::ECHO) {
        return format_simple_string(request.arguments[0]->get_string_value());
    } else if (request.command == RedisRequestCommand::SET) {
        return handle_set_request(request, cache);
    } else if (request.command == RedisRequestCommand::GET) {
        return handle_get_request(request, cache);
    } else if (request.command == RedisRequestCommand::INFO) {
        return handle_info_request(request, cache);
    } else if (request.command == RedisRequestCommand::REPLCONF) {
        return format_simple_string("OK");
    } else if (request.command == RedisRequestCommand::PSYNC) {
        return handle_psync_request(request, cache, conn);
    }
    return "";
}