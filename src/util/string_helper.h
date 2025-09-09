#ifndef _UTIL_STRING_HELPER_H_
#define _UTIL_STRING_HELPER_H_

#include <cstring>
#include <string>
#include <sstream>
#include <vector>

class strHelper {
public:

    template <class T>
    static std::string toString (const T& t);

    // split the string to array
    template <typename TYPE>
    static int splitStr(TYPE& list,
                 const std::string& str, const char* delim);

    // trim the specify char
    static std::string& trim(std::string& str, const char thechar = ' ');
    // convert type T to string
    template <typename T, typename S> 
    static const T valueOf(const S &a);

    static std::string toLower(std::string& str);
    static std::string toUpper(std::string& str);

    static std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter);

    static int replaceStringOnce( std::string& str, const char *from, const char *to, int offset);
    static bool replaceString( std::string& str, const char *from, const char *to);

    static std::string boolToString(bool val);

    static bool startsWith(const std::string& str, const std::string& prefix);
    static bool endsWith(const std::string& str, const std::string& suffix);
};


template <typename T, typename S> inline
const T strHelper::valueOf(const S& a)
{
    std::stringstream s;
    T t;
    s << a ; 
    s >> t;
    return t;
}

template <class T> inline
std::string strHelper::toString (const T& t) {
    std::stringstream ss;
    ss << t;
    return ss.str();
}


template <typename TYPE> inline
int strHelper::splitStr(TYPE& list,
                const std::string& str, const char* delim)
{
    if (str.empty())
        return 0;
        
    if (delim == NULL){
        list.push_back(str);
        return 1;
    }

    unsigned int size = strlen(delim);

    std::string::size_type prepos = 0;
    std::string::size_type pos = 0;
    int count = 0;

    for(;;) 
    {
        pos = str.find(delim, pos);
        if (pos == std::string::npos){
            if (prepos < str.size()){
                list.push_back(str.substr(prepos));
                count++;
            }
            break;
        }

        list.push_back(str.substr(prepos, pos-prepos));

        count++;
        pos += size;
        prepos = pos;
    }

    return count;
}


#endif /* _UTIL_STRING_HELPER_H_ */