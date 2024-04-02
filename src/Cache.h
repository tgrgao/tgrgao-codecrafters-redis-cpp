#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <map>
#include <chrono>

class Cache {
    public:
        const std::string get_role() {return this->role;};

        const std::string get_master_replid() {return this->master_replid;};
        const int get_master_repl_offset() {return this->master_repl_offset;};
        int add_replica_conn(int conn);
        const std::vector<int> get_replica_conns() {return this->replica_conns;};
        const std::string get_next_propagation();

        const std::string get_replica_of_host() {return this->replica_of_host;};
        const int get_replica_of_port() {return this->replica_of_port;};
        int make_master();
        int make_replica(std::string host, int port);
        int set(std::string key, std::string value, std::chrono::milliseconds::rep expiry_time_ms);
        int get(std::string key, std::string &value);

    private:
        std::string role;

        std::string master_replid;
        int master_repl_offset;
        std::vector<int> replica_conns;
        std::vector<std::string> to_propagate;

        std::string replica_of_host;
        int replica_of_port;

        std::mutex mut;
        std::condition_variable propagation_cv;

        std::map<std::string, std::pair<std::string, std::chrono::milliseconds::rep>> map;
};

#endif