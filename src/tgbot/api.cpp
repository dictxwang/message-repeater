#include "api.h"

using namespace std;

namespace tgbot {
    void TgApi::init(const string& endpoint, const string& token) {
        this->_endpoint = endpoint;
        this->_token = token;
    }

    void TgApi::init_default_endpoint(const string& token) {
        this->_endpoint = API_Endpoint;
        this->_token = token;
    }

    pair<int, string> TgApi::send_message(int64_t chat_id, const string& text) {

        vector<pair<string, string>> body_params;
        body_params.push_back(pair<string, string>("chat_id", strHelper::toString(chat_id)));
        body_params.push_back(pair<string, string>("text", text));
    
        pair<int, string> result;
        string call_result;
        vector<string> emtpy;
        vector<pair<string, string>> emtpy_params;
    
        try {
            CURLcode status = sendRequest("sendMessage", "POST", call_result, emtpy, emtpy_params, body_params);

            if (status != CURLE_OK) {
                result = pair<int, string>(status, "fail to call");
            } else {
                Json::Value json_result;
                Json::Reader reader;
                json_result.clear();
                reader.parse(call_result , json_result);
                if (!json_result.isMember("ok") || !json_result["ok"].asBool()) {
                    result = pair<int, string>(-500, "fail to send message: " + call_result);
                } else {
                    result = pair<int, string>(0, "");
                }
            }
        } catch (exception &e) {
            result = pair<int, string>(-900, "send request fail: " + string(e.what()));
        }
        return result;
    }

    CURLcode TgApi::sendRequest(const string& api_method, const string& action,  string& call_result, vector <string> &extra_header, vector<pair<string, string>> &query_params, vector<pair<string, string>> &form_params) {
        string url(this->_endpoint);
        url += "/bot";
        url += _token;
        url += "/";
        url += api_method;

        string delimiter_join = "&";
        string queryString = "";
        string formString = "";

        CURL* curl = curl_easy_init();
        CURLcode call_code;

        if (!query_params.empty()) {
            vector<string> query_param_list;
            for (pair<string, string> p : query_params) {
                query_param_list.push_back(p.first + "=" + curl_easy_escape(curl, p.second.c_str(), 0));
            }
            queryString = strHelper::joinStrings(query_param_list, delimiter_join);
        }

        if (!form_params.empty()) {
            vector<string> form_param_list;
            for (pair<string, string> p : form_params) {
                form_param_list.push_back(p.first + "=" + curl_easy_escape(curl, p.second.c_str(), 0));
            }
            formString = strHelper::joinStrings(form_param_list, delimiter_join);
            extra_header.push_back("Content-Type: application/x-www-form-urlencoded");
        }

        string fullUrl = url;
        if (queryString.size() > 0) {
            fullUrl.append("?").append(queryString);
        }

        curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str() );
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &call_result );
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

        if (extra_header.size() > 0 ) {
            
            struct curl_slist *chunk = nullptr;
            for ( size_t i = 0; i < extra_header.size(); ++i ) {
                chunk = curl_slist_append(chunk, extra_header[i].c_str() );
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        }

        if ( formString.size() > 0 || action == "POST" || action == "PUT" || action == "DELETE" ) {

            if ( action == "PUT" || action == "DELETE" ) {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, action.c_str() );
            }
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, formString.c_str() );
        }

        call_code = curl_easy_perform(curl);
        
        if (curl != nullptr) {
            curl_easy_cleanup(curl);
        }

        return call_code;
    }

    static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, string* userData) {
        size_t totalSize = size * nmemb;
        userData->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }
}