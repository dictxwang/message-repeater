#include "config.h"

bool BaseConfig::load_config_file(const char* inputfile) {
    std::ifstream ifile(inputfile);
    if (!ifile) {
        std::cerr << "Error: Cannot open config file: " << inputfile << std::endl;
        return false;
    }

    ifile.seekg (0, ifile.end);
    int length = ifile.tellg();
    ifile.seekg (0, ifile.beg);

    char * buffer = new char[length+1];
    ifile.read(buffer, length);
    buffer[length] = 0;
    ifile.close();

    this->doc_.clear();
    Json::Reader reader;
    try {
        reader.parse(buffer, this->doc_);
    } catch (std::exception &e) {
        std::cerr << "Error: JSON parse error: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool Config::load_config(const char* inputfile) {
    bool loadFileResult = BaseConfig::load_config_file(inputfile);
    if (!loadFileResult) {
        return false;
    }
    // Parse common configuration keys
    this->logger_name = doc_["logger_name"].asString();
    this->logger_file_path = doc_["logger_file_path"].asString();
    this->logger_level = doc_["logger_level"].asInt();
    this->logger_max_files = doc_["logger_max_files"].asInt();

    return true;
}