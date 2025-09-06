#include "test_config.h"

bool TestConfig::loadTestConfig(const char* inputfile) {
    bool loadFileResult = Config::load_config(inputfile);
    if (!loadFileResult) {
        return false;
    }

    // Parse own configuration properties
    this->version = this->doc_["version"].asString();

    return true;
}