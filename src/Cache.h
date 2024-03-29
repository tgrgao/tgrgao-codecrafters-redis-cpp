#ifndef CACHE_H
#define CACHE_H

#include <mutex>
#include <map>
#include <chrono>

class Cache {
    public:
        int set(std::string key, std::string value, std::chrono::milliseconds::rep expiry_time_ms);
        int get(std::string key, std::string &value);

    private:
        struct std::mutex mut;
        std::map<std::string, std::pair<std::string, std::chrono::milliseconds::rep>> map;
};

#endif