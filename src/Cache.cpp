#include "Cache.h"

int Cache::make_master() {
    role = "master";

    master_replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
    master_repl_offset = 0;

    replica_of_host = "";
    replica_of_port = -1;

    return 0;
}

int Cache::make_replica(std::string host, int port) {
    role = "slave";

    replica_of_host = host;
    replica_of_port = port;

    master_replid = "";
    master_repl_offset = -1;

    return 0;
}

int Cache::set(std::string key, std::string value, std::chrono::milliseconds::rep expiry_time_ms) {
    std::lock_guard<std::mutex> lock(mut);
    map[key] = std::pair(value, expiry_time_ms);
    if (role == "master") {
        std::string propagation_str;
        if (expiry_time_ms != -1) {
            propagation_str = "*5\r\n$3\r\nSET\r\n$" + std::to_string(key.length()) + "\r\n"+ key + "\r\n$" + std::to_string(value.length()) + "\r\n" + value + "\r\n$2\r\npx\r\n$" + std::to_string(std::to_string(expiry_time_ms).length()) + "\r\n" + std::to_string(expiry_time_ms) + "\r\n";
        } else {
            propagation_str = "*3\r\n$3\r\nSET\r\n$" + std::to_string(key.length()) + "\r\n"+ key + "\r\n$" + std::to_string(value.length()) + "\r\n" + value + "\r\n";
        }
        to_propagate.push_back(propagation_str);
        propagation_cv.notify_one();
    }
    return 0;
}

int Cache::get(std::string key, std::string &value) {
    std::lock_guard<std::mutex> lock(mut);

    if (map.find(key) != map.end() && (map[key].second < 0 || map[key].second > std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())) { // check key exists in cache and that, if there is an expiry time, it is not expired
        value = map[key].first;
        return 0;
    } else {
        return -1;
    }
}

int Cache::add_replica_conn(int conn) {
    std::lock_guard<std::mutex> lock(mut);
    replica_conns.push_back(conn);
    return 0;
}

const std::string Cache::get_next_propagation() {
    std::unique_lock<std::mutex> lock(mut);
    propagation_cv.wait(lock, [this]{ return !to_propagate.empty(); });
    std::string ret = to_propagate.back();
    to_propagate.pop_back();
    return ret;
}