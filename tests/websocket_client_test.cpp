#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/connect.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <json/json.h>
#include "logger.hpp"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;

class WebSocketClient {
public:
    WebSocketClient() 
        : ioc_()
        , resolver_(ioc_)
        , ws_(ioc_)
        , connected_(false)
    {
        LOG_INFO("WebSocketClient initialized");
    }

    ~WebSocketClient() {
        close();
        if (io_thread_ && io_thread_->joinable()) {
            LOG_DEBUG("Joining IO thread");
            io_thread_->join();
        }
        LOG_INFO("WebSocketClient destroyed");
    }

    void connect(const std::string& host, const std::string& port) {
        try {
            LOG_INFO("Connecting to %s:%s", host.c_str(), port.c_str());
            
            auto const results = resolver_.resolve(host, port);
            
            auto endpoint = asio::connect(ws_.next_layer(), results);
            LOG_DEBUG("Connected to endpoint: %s:%d", endpoint.address().to_string().c_str(), endpoint.port());
            
            ws_.set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req) {
                    req.set(beast::http::field::user_agent,
                        std::string(BOOST_BEAST_VERSION_STRING) +
                            " deribit-client");
                }));
            
            std::string target = "/";
            ws_.handshake(host, target);
            connected_ = true;
            
            LOG_INFO("Successfully connected to WebSocket server at %s:%s", host.c_str(), port.c_str());
            
            io_thread_ = std::make_unique<std::thread>([this]() {
                this->read_messages();
            });
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error connecting to WebSocket server: %s", e.what());
        }
    }

    void subscribe_to_symbol(const std::string& symbol) {
        if (!connected_) {
            LOG_WARNING("Cannot subscribe to %s: Not connected to WebSocket server", symbol.c_str());
            return;
        }
        
        Json::Value request;
        request["action"] = "subscribe";
        request["symbol"] = symbol;

        std::string message = Json::FastWriter().write(request);
        
        try {
            LOG_DEBUG("Sending subscription request: %s", message.c_str());
            ws_.write(asio::buffer(message));
            LOG_INFO("Successfully sent subscription request for %s", symbol.c_str());
        } catch (const std::exception& e) {
            LOG_ERROR("Error sending subscription for %s: %s", symbol.c_str(), e.what());
        }
    }

    void close() {
        if (connected_) {
            try {
                LOG_INFO("Closing WebSocket connection");
                connected_ = false;
                
                ws_.close(websocket::close_code::normal);
                LOG_INFO("WebSocket connection closed successfully");
                
            } catch (const std::exception& e) {
                LOG_ERROR("Error closing WebSocket connection: %s", e.what());
            }
        }
    }

private:
    void read_messages() {
        LOG_INFO("Started message reading thread");
        try {
            while (connected_) {
                beast::flat_buffer buffer;
                LOG_DEBUG("Waiting for incoming message");
                ws_.read(buffer);
                
                std::string message = beast::buffers_to_string(buffer.data());
                LOG_INFO("Received message: %s", message.c_str());
            }
        } catch (const beast::system_error& e) {
            if (e.code() == websocket::error::closed) {
                LOG_INFO("WebSocket connection closed by server");
            } else {
                LOG_ERROR("System error in WebSocket: %s", e.what());
            }
            connected_ = false;
        } catch (const std::exception& e) {
            LOG_ERROR("Error reading from WebSocket: %s", e.what());
            connected_ = false;
        }
        LOG_INFO("Message reading thread terminated");
    }

    asio::io_context ioc_;
    tcp::resolver resolver_;
    websocket::stream<tcp::socket> ws_;
    std::unique_ptr<std::thread> io_thread_;
    std::atomic<bool> connected_;
};

int main() {
    try {
        deribit::Logger::instance().set_level(deribit::LogLevel::DEBUG);
        deribit::Logger::instance().set_log_file("websocket_client.log");
        
        LOG_INFO("Application started");
        
        WebSocketClient client;
        LOG_INFO("Connecting to WebSocket server...");
        
        client.connect("localhost", "8080");
        
        client.subscribe_to_symbol("BTC-PERPETUAL");
        
        LOG_INFO("Press Enter to exit");
        std::cin.get();
        LOG_INFO("Application shutting down");
        
    } catch (const std::exception& e) {
        LOG_CRITICAL("Fatal error: %s", e.what());
    }
    
    return 0;
}