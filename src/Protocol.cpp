#include "Protocol.h"

std::string format_bulk_string(std::string s) {
    return "$" + std::to_string(s.length()) + "\r\n" + s + "\r\n";
}