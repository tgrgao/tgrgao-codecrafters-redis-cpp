#include <mutex>
#include <map>
#include <chrono>

struct Cache {
    struct std::mutex mut;
    std::map<std::string, std::pair<std::string, std::chrono::milliseconds::rep>> map;
};