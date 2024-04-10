#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <variant>
#include <memory>

class RedisExpression;

class RedisExpression {
    public:
        enum RedisType {
            Undetermined,
            NoMatch,
            SimpleString,
            BulkString,
            Array
        };
        RedisType type;
        bool finished;
        int length;
        std::variant<std::string, int, std::vector<std::shared_ptr<RedisExpression>>> value;

        RedisExpression(std::string& s);
        ~RedisExpression();
};

// std::string format_bulk_string(std::string s);

#endif