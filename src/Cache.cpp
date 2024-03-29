#include "Cache.h"

int Cache::set(std::string key, std::string value, std::chrono::milliseconds::rep expiry_time_ms) {
    mut.lock();
    map[key] = std::pair(value, expiry_time_ms);
    mut.unlock();
    return 0;
}

int Cache::get(std::string key, std::string &value) {
    mut.lock();

    if (map.find(key) != map.end() && (map[key].second < 0 || map[key].second > std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())) { // check key exists in cache and that, if there is an expiry time, it is not expired
        value = map[key].first;
        mut.unlock();
        return 0;
    } else {
        mut.unlock();
        return -1;
    }
}