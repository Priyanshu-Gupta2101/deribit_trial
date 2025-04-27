#include "market_data.hpp"
#include <iostream>
#include <cpprest/uri_builder.h>
#include <cpprest/json.h>
#include <cpprest/asyncrt_utils.h>

namespace deribit {

MarketData::MarketData(Config& config)
    : config_(config)
    , client_(web::uri(utility::conversions::to_string_t(config.BASE_URL)))
{}

web::json::value MarketData::get_orderbook(const std::string& instrument_name, int depth) {
    web::uri_builder builder(U("/public/get_order_book"));
    builder.append_query(U("instrument_name"), instrument_name)
           .append_query(U("depth"), depth);

    try {
        auto response = client_.request(web::http::methods::GET, builder.to_string()).get();
        if (response.status_code() == web::http::status_codes::OK) {
            std::cout << "Retrieved orderbook for instrument: " << instrument_name << std::endl;
            return response.extract_json().get();
        } else {
            std::cout << "Failed to get orderbook for instrument: " << instrument_name << ". Status code: " << response.status_code() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting orderbook for instrument " << instrument_name << ": " << e.what() << std::endl;
    }
    return web::json::value::null();
}

web::json::value MarketData::get_ticker(const std::string& instrument_name) {
    web::uri_builder builder(U("/public/ticker"));
    builder.append_query(U("instrument_name"), instrument_name);

    try {
        auto response = client_.request(web::http::methods::GET, builder.to_string()).get();
        if (response.status_code() == web::http::status_codes::OK) {
            std::cout << "Retrieved ticker for instrument: " << instrument_name << std::endl;
            return response.extract_json().get();
        } else {
            std::cout << "Failed to get ticker for instrument: " << instrument_name << ". Status code: " << response.status_code() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting ticker for instrument " << instrument_name << ": " << e.what() << std::endl;
    }
    return web::json::value::null();
}

web::json::value MarketData::get_instruments(const std::string& currency, const std::string& kind) {
    web::uri_builder builder(U("/public/get_instruments"));
    builder.append_query(U("currency"), currency)
           .append_query(U("kind"), kind);

    try {
        auto response = client_.request(web::http::methods::GET, builder.to_string()).get();
        if (response.status_code() == web::http::status_codes::OK) {
            std::cout << "Retrieved instruments for currency: " << currency << ", kind: " << kind << std::endl;
            return response.extract_json().get();
        } else {
            std::cout << "Failed to get instruments for currency: " << currency << ", kind: " << kind << ". Status code: " << response.status_code() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting instruments for currency " << currency << ", kind " << kind << ": " << e.what() << std::endl;
    }
    return web::json::value::null();
}

web::json::value MarketData::get_options_instruments(const std::string& currency) {
    web::uri_builder builder(U("/public/get_instruments"));
    builder.append_query(U("currency"), currency)
           .append_query(U("kind"), "option");

    try {
        auto response = client_.request(web::http::methods::GET, builder.to_string()).get();
        if (response.status_code() == web::http::status_codes::OK) {
            std::cout << "Retrieved options instruments for currency: " << currency << std::endl;
            return response.extract_json().get();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting options instruments: " << e.what() << std::endl;
    }
    return web::json::value::null();
}

}