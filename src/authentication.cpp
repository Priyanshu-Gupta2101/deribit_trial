#include "authentication.hpp"
#include <cpprest/json.h>
#include <cpprest/asyncrt_utils.h>

namespace deribit {

Authentication::Authentication(Config& config)
    : config_(config)
    , is_authenticated_(false)
    , client_(web::uri(utility::conversions::to_string_t(config.BASE_URL)))
{}

bool Authentication::authenticate() {
    web::uri_builder builder(U("/public/auth"));
    builder.append_query(U("client_id"), utility::conversions::to_string_t(config_.client_id))
           .append_query(U("client_secret"), utility::conversions::to_string_t(config_.client_secret))
           .append_query(U("grant_type"), U("client_credentials"));

    try {
        auto response = client_.request(web::http::methods::GET, builder.to_string()).get();
        
        if (response.status_code() == web::http::status_codes::OK) {
            auto json = response.extract_json().get();
            auto& result = json.at(U("result"));
            auto& access_token = result.at(U("access_token"));
            config_.access_token = utility::conversions::to_utf8string(access_token.as_string());
            is_authenticated_ = true;
            return true;
        }
    } catch (const std::exception& e) {
        is_authenticated_ = false;
    }
    return false;
}

std::string Authentication::get_access_token() const {
    return config_.access_token;
}

bool Authentication::is_authenticated() const {
    return is_authenticated_;
}

void Authentication::refresh_token() {
    authenticate();
}

} // namespace deribit