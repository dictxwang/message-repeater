#ifndef _CONFIG_TEST_CONFIG_H_
#define _CONFIG_TEST_CONFIG_H_

#include "config.h"

class TestConfig : public Config
{
    public:
        TestConfig() {}
        ~TestConfig() {}

    public:
        bool loadTestConfig(const char* inputfile);

    public:
        std::string version;
};
#endif