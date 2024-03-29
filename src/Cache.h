#include <mutex>
#include <map>
#include <ctime>

struct Cache {
    struct std::mutex mut;
    std::map<std::string, std::pair<std::string, std::time_t>> map;
};