#ifndef API_H
#define API_H

#include <vector>
#include <string>

#include "Cache.h"
#include "resp-parser/Protocol.h"

enum RedisRequestCommand {
    PING,
    ECHO,
    SET,
    GET,
    INFO,
    REPLCONF,
    PSYNC
};

struct RedisRequest {
    RedisRequestCommand command;
    std::vector<std::unique_ptr<RedisExpression>> arguments;
};

int parse_redis_request(RedisRequest& request, RedisExpression& expression);

std::string handle_request(RedisRequest& request, Cache& cache, int conn);

#endif