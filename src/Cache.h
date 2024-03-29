#ifndef CACHE_H
#define CACHE_H

#include <mutex>
#include <map>
#include <chrono>

class Cache {
    public:
        const std::string get_role() {return this->role;};
        int make_master();
        int make_replica(std::string host, int port);
        int set(std::string key, std::string value, std::chrono::milliseconds::rep expiry_time_ms);
        int get(std::string key, std::string &value);

    private:
        std::string role;
        std::string replica_of_host;
        int replicat_of_port;
        struct std::mutex mut;
        std::map<std::string, std::pair<std::string, std::chrono::milliseconds::rep>> map;
};

#endif