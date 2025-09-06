#ifndef _CONFIG_CONFIG_H_
#define _CONFIG_CONFIG_H_   

#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include "json/json.h"

using std::string;

class BaseConfig
{
public:
    BaseConfig() {};
    virtual ~BaseConfig() {}

protected:
    bool load_config_file(const char* inputfile);

protected:
    Json::Value doc_;
};


class Config : public BaseConfig
{
public:
    Config() {};
    virtual ~Config() {}

protected:
    bool load_config(const char* inputfile);

public:
    string logger_name;
    int logger_level;
    int logger_max_files;
    string logger_file_path;
};


#endif