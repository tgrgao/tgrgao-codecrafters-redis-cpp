#include "Protocol.h"

#include <iostream>

RedisExpression::RedisExpression(std::string& s) {
    if (s.empty()) {
        type = RedisType::Undetermined;
        value = 0;
        return;
    }

    switch (s[0]) {
        case '+':
            type = RedisType::SimpleString;
            break;
        case '$':
            type = RedisType::BulkString;
            break;
        case '*':
            type = RedisType::Array;
            break;
        default:
            type = RedisType::NoMatch;
            value = 0;
            return;
    }

    int at = 1;

    int next_rn = s.find("\r\n", at);
    if (next_rn == std::string::npos) {
        type = RedisType::Undetermined;
        return;
    }

    if (type == RedisType::SimpleString) {
        value = s.substr(at, next_rn - at);
        finished = true;
        length = std::get<std::string>(value).length();
        s = s.substr(next_rn + 2);
        return;
    }
    
    length = std::stoi(s.substr(at, next_rn - at));
    at = next_rn + 2;

    switch (type) {
        case RedisType::BulkString:
            next_rn = s.find("\r\n", at);
            if (next_rn == std::string::npos) {
                value = s.substr(at);
                finished = false;
                s = "";
                return;
            }
            value = s.substr(at, next_rn);
            finished = true;
            s = s.substr(next_rn + 2);
            return;
            break;
        case RedisType::Array:
            value = std::vector<std::shared_ptr<RedisExpression>>{};
            for (int i = 0; i < length; ++i) {
                RedisExpression expression(s);
                if (expression.type != RedisExpression::Undetermined) {
                    std::get<std::vector<RedisExpression>>(value).push_back(s);
                }
            }
            if (std::get<std::vector<RedisExpression>>(value).size() == length && std::get<std::vector<RedisExpression>>(value)[length - 1].finished == true) {
                finished = true;
            } else {
                finished = false;
            }
            return;
            break;
        default:
            std::cerr << "Not reachable";
            return;
    }
}

RedisExpression::~RedisExpression() {
    if (type == RedisType::Array) {
        auto& vec = std::get<std::vector<std::shared_ptr<RedisExpression>>>(value);
        for (const auto& ptr : vec) {
            ptr.reset(); // Release shared_ptr reference
        }
    }
}




// std::string format_bulk_string(std::string s) {
//     return "$" + std::to_string(s.length()) + "\r\n" + s + "\r\n";
// }

// int read_file(char *buf, int max_len, File& file) {
//     std::string buf_string = std::string(buf, max_len);
//     if (buf_string[0] != '$') {
//         return 0; // error: 0 bytes read
//     }

//     int file_size = std::stoi(buf_string.substr(1, buf_string.find("\r\n") - 1));
//     int file_start = buf_string.find("\r\n") + 2;
//     if (max_len - file_start < file_size) {
//         file.content = buf_string.substr(file_start, max_len - file_start);
//         return file_size - (max_len - file_start); // negative: how many bytes still need to be read for the file
//     } else {
//         file.content = buf_string.substr(file_start, file_size);
//         return file_start + file_size; // where in buffer we ended after reading file
//     }
// }