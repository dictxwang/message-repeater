#ifndef _UTIL_COMMON_TOOL_H_
#define _UTIL_COMMON_TOOL_H_

#include <sys/time.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <random>
#include <iomanip>
#include "json/json.h"

namespace common_tools {

    static double str_to_dobule(const Json::Value& value) {
        std::string text = value.asString();
        if (text == "") {
            return 0;
        } else {
            return std::stod(text);
        }
    }

    static int str_to_int(const Json::Value& value) {
        std::string text = value.asString();
        if (text == "") {
            return 0;
        } else {
            return std::stoi(text);
        }
    }

    static double str_to_uint64(const Json::Value& value) {
        std::string text = value.asString();
        if (text == "") {
            return 0;
        } else {
            char* endptr;
            return std::strtoul(text.c_str(), &endptr, 10);
        }
    }

    static std::string node_as_string(const Json::Value& node, std::string child) {
        if (node.isMember(child)) {
            return node[child].asString();
        } else {
            return "";
        }
    }
    
    static double node_as_double(const Json::Value& node, std::string child) {
        if (node.isMember(child)) {
            return str_to_dobule(node[child]);
        } else {
            return 0;
        }
    }

    static std::string serialize_json_value(Json::Value& value) {
        Json::StreamWriterBuilder writer;
        std::string jsonString = Json::writeString(writer, value);
        return jsonString;
    }

    //---------------------------------
    static uint64_t get_current_epoch() {

        struct timeval tv;
        gettimeofday(&tv, NULL); 

        return tv.tv_sec ;
    }

    //---------------------------------
    static uint64_t get_current_ms_epoch() {

        struct timeval tv;
        gettimeofday(&tv, NULL);

        return tv.tv_sec * 1000 + tv.tv_usec / 1000 ;

    }
    
    //---------------------------------
    static uint64_t get_current_micro_epoch() {

        struct timeval tv;
        gettimeofday(&tv, NULL);

        return tv.tv_sec * 1000000ULL + tv.tv_usec;
    }
}

#endif