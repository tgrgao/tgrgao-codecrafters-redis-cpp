#ifndef API_H
#define API_H

#include <vector>
#include <string>

#include "Cache.h"

enum RedisRequestCommand {
    PING,
    ECHO,
    SET,
    GET,
    INFO,
    REPLCONF
};

struct RedisRequest {
    RedisRequestCommand command;
    std::vector<std::string> arguments;
};

int parse_redis_request(RedisRequest& request, char *request_bytes, int len);

std::string handle_request(RedisRequest& request, Cache& cache);

std::string format_bulk_string(std::string s);

#endif