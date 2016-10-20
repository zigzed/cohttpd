/**
 * create: 2016-09-30
 * author: mr.wclong@yahoo.com
 */
#include "request.hpp"

#include <algorithm>

namespace coh {

    http_request::http_request()
        : headers_completed_(false)
        , message_completed_(false)
        // state_field_value_ 初始化为 true，这样首先处理的就应该是 field 的 key
        , state_field_value_(true)
        , bodysz_(0)
    {
        strurl_.clear();
        memset(&decurl_, 0, sizeof(decurl_));
        fields_.clear();
        bodybf_.clear();

        http_parser_init(&parsed_, HTTP_REQUEST);
        parsed_.data = this;
    }

    void http_request::reset()
    {
        headers_completed_ = false;
        message_completed_ = false;
        state_field_value_ = true;
        strurl_.clear();
        memset(&decurl_, 0, sizeof(decurl_));
        fields_.clear();
        bodybf_.clear();
        bodysz_ = 0;
    }

    struct string_pair_less {
        bool operator()(const std::pair<std::string, std::string >& a, const std::pair<std::string, std::string >& b) const {
            return a.first < b.first;
        }
    };

    void http_request::sorthdr()
    {
        std::sort(fields_.begin(), fields_.end(), string_pair_less());
    }

    const std::pair<std::string, std::string >* http_request::operator[](size_t index) const
    {
        if(index >= fields_.size())
            return nullptr;
        return &fields_[index];
    }

    const std::string* http_request::operator[](const char* field) const
    {
        std::pair<std::string, std::string > key = std::make_pair(field, std::string());
        fields_t::const_iterator it = std::lower_bound(fields_.begin(), fields_.end(), key, string_pair_less());
        if(it == fields_.end() || it->first != key.first)
            return nullptr;
        return &it->second;
    }


}
