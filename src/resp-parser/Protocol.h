#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <memory>
#include <variant>
#include <vector>
#include <map>

class RedisExpression;

class RedisExpression {
    public:
        enum RedisType {
            Init,
            NoMatch,
            SimpleString,
            BulkString,
            Integer,
            Array
        };

        RedisType type;
        std::variant<int, std::string> value;
        std::vector<std::unique_ptr<RedisExpression>> array;
        bool finished;
        int length;

        RedisExpression();
        void consume_input(std::string& s);

        std::string to_string();

        int get_int_value();
        std::string get_string_value();
};

std::string format_simple_string(std::string s);
std::string format_bulk_string(std::string s);

#endif