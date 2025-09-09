#include "string_helper.h"
#include <algorithm>
#include <cctype>

std::string strHelper::boolToString(bool val)
{
    if (val) {
        return "true";
    } else {
        return "false";
    }
}

std::string strHelper::toUpper(std::string& original)
{
    std::string str = std::string(original);
    std::transform(str.begin(), str.end(), str.begin(), [](char& c){
        return std::toupper(c);
    });
	return str;
}

std::string strHelper::toLower(std::string& original)
{
    std::string str = std::string(original);
    std::transform(str.begin(), str.end(), str.begin(), [](char& c){
        return std::tolower(c);
    });
	return str;
}


std::string& strHelper::trim(std::string& str, const char thechar)
{
    if (str.empty()) {
        return str;
    }

    std::string::size_type pos = str.find_first_not_of(thechar);
    if (pos != std::string::npos) {
        str.erase(0, pos);
    }
    pos = str.find_last_of(thechar);
    if (pos != std::string::npos) {
        str.erase(str.find_last_not_of(thechar) + 1);
    }
    return str;
}

//--------------------------------
int strHelper::replaceStringOnce( std::string& str, const char *from, const char *to, int offset) {

    size_t start_pos = str.find(from, offset);
    if( start_pos == std::string::npos ) {
        return 0;
    }
    str.replace(start_pos, strlen(from), to);
    return start_pos + strlen(to);
}

//--------------------------------
bool strHelper::replaceString( std::string& str, const char *from, const char *to) {

    bool found = false;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, strlen( from ), to);
        found = true;
        start_pos += strlen(to);
    }
    return found;
}


std::string strHelper::joinStrings(const std::vector<std::string>& strings, const std::string& delimiter) {
    std::ostringstream oss;
    for (size_t i = 0; i < strings.size(); ++i) {
        oss << strings[i];
        if (i != strings.size() - 1) { // Add delimiter between strings
            oss << delimiter;
        }
    }
    return oss.str();
}

bool strHelper::startsWith(const std::string& str, const std::string& prefix) {
      return str.length() >= prefix.length() && str.compare(0, prefix.length(), prefix) == 0;
}

bool strHelper::endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}