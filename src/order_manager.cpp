#include "order_manager.hpp"
#include <cpprest/json.h>
#include <cpprest/asyncrt_utils.h>
#include <cpprest/uri_builder.h>
#include <performance_metrics.hpp>

namespace deribit
{

    OrderManager::OrderManager(Config &config)
        : config_(config), client_(web::uri(utility::conversions::to_string_t(config.BASE_URL)))
    {
    }

    web::http::http_request OrderManager::create_authenticated_request(
        web::http::method method,
        const std::string &path)
    {
        web::http::http_request request(method);
        request.headers().add(
            U("Authorization"), 
            U("Bearer ") + utility::conversions::to_string_t(config_.access_token)
        );
        return request;
    }

    std::string OrderManager::place_buy_order(const OrderParams &params)
    {
        START_TIMING("buy_order_placement");
        web::uri_builder builder(U("/private/buy"));
        builder.append_query(U("amount"), params.amount)
            .append_query(U("instrument_name"), params.instrument_name)
            .append_query(U("type"), params.type);

        if (params.type == "limit")
        {
            builder.append_query(U("price"), params.price);
        }

        auto request = create_authenticated_request(
            web::http::methods::GET,
            utility::conversions::to_utf8string(builder.to_string())
        );

        try
        {
            auto response = client_.request(request).get();
            END_TIMING("buy_order_placement");
            if (response.status_code() == web::http::status_codes::OK)
            {
                auto json = response.extract_json().get();
                auto& result = json.at(U("result"));
                auto& order = result.at(U("order"));
                return utility::conversions::to_utf8string(order.at(U("order_id")).as_string());
            }
        }
        catch (const std::exception &e)
        {
            END_TIMING("buy_order_placement");
            std::cout << e.what() << std::endl;
        }
        return "";
    }

    std::string OrderManager::place_sell_order(const OrderParams& params) {
        START_TIMING("sell_order_placement");
        web::uri_builder builder(U("/private/sell"));
        builder.append_query(U("advanced"), "usd")
            .append_query(U("amount"), params.amount)
            .append_query(U("instrument_name"), params.instrument_name);
                    
        if (params.type == "limit") {
            builder.append_query(U("price"), params.price);
        }

        builder.append_query(U("type"), params.type);

        auto request = create_authenticated_request(web::http::methods::GET, utility::conversions::to_utf8string(builder.to_string()));

        try {
            auto response = client_.request(request).get();
            END_TIMING("sell_order_placement");
            if (response.status_code() == web::http::status_codes::OK) {
                auto json = response.extract_json().get();
                auto& result = json.at(U("result"));
                auto& order = result.at(U("order"));
                return utility::conversions::to_utf8string(order.at(U("order_id")).as_string());
            }
        } catch (const std::exception& e) {
            END_TIMING("sell_order_placement");
            std::cout << e.what() << std::endl; 
        }
        return "";
    }

    bool OrderManager::cancel_order(const std::string &order_id)
    {
        web::uri_builder builder(U("/private/cancel"));
        builder.append_query(U("order_id"), order_id);

        auto request = create_authenticated_request(web::http::methods::GET, utility::conversions::to_utf8string(builder.to_string()));

        try
        {
            auto response = client_.request(request).get();
            return response.status_code() == web::http::status_codes::OK;
        }
        catch (const std::exception &e)
        {
            std::cout << e.what() << std::endl;
        }
        return false;
    }

    bool OrderManager::modify_order(const std::string &order_id, double new_amount, double new_price)
    {
        web::uri_builder builder(U("/private/edit"));
        builder.append_query(U("order_id"), order_id)
            .append_query(U("amount"), new_amount)
            .append_query(U("price"), new_price);

        auto request = create_authenticated_request(web::http::methods::GET, utility::conversions::to_utf8string(builder.to_string()));

        try
        {
            auto response = client_.request(request).get();
            return response.status_code() == web::http::status_codes::OK;
        }
        catch (const std::exception &e)
        {
            std::cout << e.what() << std::endl;
        }
        return false;
    }

    web::json::value OrderManager::get_positions(const std::string &currency, const std::string &kind)
    {
        web::uri_builder builder(U("/private/get_positions"));
        builder.append_query(U("currency"), currency)
            .append_query(U("kind"), kind);

        auto request = create_authenticated_request(web::http::methods::GET, utility::conversions::to_utf8string(builder.to_string()));

        try
        {
            auto response = client_.request(request).get();
            if (response.status_code() == web::http::status_codes::OK)
            {
                return response.extract_json().get();
            }
        }
        catch (const std::exception &e)
        {
            std::cout << e.what() << std::endl;
        }
        return web::json::value::null();
    }

}