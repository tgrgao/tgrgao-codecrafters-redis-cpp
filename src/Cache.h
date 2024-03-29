#include <mutex>
#include <map>

struct Cache {
    struct std::mutex mut;
    std::map<std::string, std::string> map;
};