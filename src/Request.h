#include <vector>
#include <string>

enum RedisRequestCommand {
    PING,
    ECHO
};

struct RedisRequest {
    RedisRequestCommand command;
    std::vector<std::string> arguments;
};

int parse_redis_request(RedisRequest *request, char *request_bytes, int len);