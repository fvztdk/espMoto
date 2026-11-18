#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

template <typename T, typename L, typename H>
constexpr auto constrain(T amt, L low, H high) {
    return (amt < static_cast<T>(low))
               ? static_cast<T>(low)
               : ((amt > static_cast<T>(high)) ? static_cast<T>(high) : amt);
}

inline uint32_t millis() {
    static const auto start = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
}

class String {
public:
    String() = default;
    String(const char* s) : _str(s ? s : "") {}
    String(const std::string& s) : _str(s) {}
    String(int v) : _str(std::to_string(v)) {}
    String(unsigned int v) : _str(std::to_string(v)) {}
    String(long v) : _str(std::to_string(v)) {}
    String(unsigned long v) : _str(std::to_string(v)) {}
    String(float v, int decimalPlaces = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimalPlaces, static_cast<double>(v));
        _str = buf;
    }

    const char* c_str() const { return _str.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(_str.length()); }
    bool isEmpty() const { return _str.empty(); }

    int indexOf(char c, unsigned int fromIndex = 0) const {
        if (fromIndex >= _str.length()) return -1;
        size_t pos = _str.find(c, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    String substring(unsigned int beginIndex) const {
        if (beginIndex >= _str.length()) return String("");
        return String(_str.substr(beginIndex));
    }

    String substring(unsigned int beginIndex, unsigned int endIndex) const {
        if (beginIndex >= _str.length() || endIndex <= beginIndex) return String("");
        return String(_str.substr(beginIndex, endIndex - beginIndex));
    }

    long toInt() const {
        if (_str.empty()) return 0;
        return std::strtol(_str.c_str(), nullptr, 10);
    }

    bool operator==(const String& rhs) const { return _str == rhs._str; }
    bool operator!=(const String& rhs) const { return _str != rhs._str; }
    bool operator==(const char* rhs) const { return _str == (rhs ? rhs : ""); }
    bool operator!=(const char* rhs) const { return _str != (rhs ? rhs : ""); }

    String operator+(const String& rhs) const { return String(_str + rhs._str); }
    String operator+(const char* rhs) const { return String(_str + (rhs ? rhs : "")); }
    String& operator+=(const String& rhs) {
        _str += rhs._str;
        return *this;
    }
    String& operator+=(const char* rhs) {
        if (rhs) _str += rhs;
        return *this;
    }

    friend String operator+(const char* lhs, const String& rhs) {
        return String(std::string(lhs ? lhs : "") + rhs._str);
    }

private:
    std::string _str;
};

#endif // ARDUINO
