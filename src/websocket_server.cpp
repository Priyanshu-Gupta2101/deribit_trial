#include "websocket_server.hpp"
#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <json/json.h>
#include "logger.hpp"

namespace deribit {

WebSocketSession::WebSocketSession(
    boost::asio::ip::tcp::socket socket,
    message_handler on_message
) : ws_(std::move(socket))
  , on_message_(std::move(on_message))
{
    LOG_DEBUG("WebSocketSession created");
}

void WebSocketSession::start() {
    LOG_INFO("Starting WebSocketSession");
    ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(
        boost::beast::role_type::server));

    ws_.async_accept(
        [self = shared_from_this()](boost::system::error_code ec) {
            if(ec) {
                LOG_ERROR("Error accepting websocket: %s", ec.message().c_str());
                return;
            }
            LOG_INFO("WebSocket connection accepted");
            self->do_read();
        });
}

void WebSocketSession::do_read() {
    LOG_DEBUG("Setting up async read");
    ws_.async_read(
        buffer_,
        [self = shared_from_this()](
            boost::system::error_code ec,
            std::size_t bytes_transferred) {
                self->on_read(ec, bytes_transferred);
        });
}

void WebSocketSession::on_read(
    boost::system::error_code ec,
    std::size_t bytes_transferred) {
    if(ec == boost::beast::websocket::error::closed) {
        LOG_INFO("WebSocket connection closed");
        return;
    }

    if(ec) {
        LOG_ERROR("Error reading from websocket: %s", ec.message().c_str());
        return;
    }

    std::string message = boost::beast::buffers_to_string(buffer_.data());
    LOG_DEBUG("Read %zu bytes: %s", bytes_transferred, message.c_str());
    buffer_.consume(buffer_.size());
    
    on_message_(shared_from_this(), message);

    do_read();
}

void WebSocketSession::send(const std::string& message) {
    LOG_DEBUG("Queueing message for send: %s", message.c_str());
    boost::asio::post(
        ws_.get_executor(),
        [self = shared_from_this(), message]() {
            std::lock_guard<std::mutex> lock(self->write_mutex_);
            
            self->ws_.binary(true);
            
            self->ws_.async_write(
                boost::asio::buffer(message),
                [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                    self->on_write(ec, bytes_transferred);
                });
        });
}

void WebSocketSession::on_write(
    boost::system::error_code ec,
    std::size_t bytes_transferred) {
    if(ec) {
        LOG_ERROR("Error writing to websocket: %s", ec.message().c_str());
        return;
    }
    
    LOG_DEBUG("Successfully wrote %zu bytes", bytes_transferred);
}

void WebSocketSession::close() {
    LOG_INFO("Closing WebSocketSession");
    boost::asio::post(
        ws_.get_executor(),
        [self = shared_from_this()]() {
            self->ws_.async_close(
                boost::beast::websocket::close_code::normal,
                [](boost::system::error_code ec) {
                    if(ec) {
                        LOG_ERROR("Error closing websocket: %s", ec.message().c_str());
                    } else {
                        LOG_INFO("WebSocket closed successfully");
                    }
                });
        });
}

WebsocketServer::WebsocketServer(Config& config)
    : config_(config)
    , ioc_()
    , acceptor_(ioc_)
    , running_(false)
    , deribit_connected_(false)
    , ssl_ctx_(boost::asio::ssl::context::tlsv12_client)
{
    LOG_INFO("WebsocketServer initializing");
    ssl_ctx_.set_default_verify_paths();
    ssl_ctx_.set_verify_mode(boost::asio::ssl::verify_peer);
    LOG_DEBUG("SSL context initialized");
}

WebsocketServer::~WebsocketServer() {
    LOG_INFO("WebsocketServer destructor called");
    stop();
}

void WebsocketServer::run(uint16_t port) {
    try {
        LOG_INFO("Starting WebsocketServer on port %u", port);
        running_ = true;
        
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("0.0.0.0"), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        
        LOG_INFO("WebSocket server listening on port %u", port);
        
        do_accept();
        
        init_deribit_connection();
        
        unsigned int thread_count = std::thread::hardware_concurrency();
        LOG_INFO("Starting %u IO service threads", thread_count);
        
        server_threads_.reserve(thread_count);
        for(auto i = 0u; i < thread_count; ++i) {
            server_threads_.emplace_back([this] { 
                LOG_DEBUG("IO service thread started");
                ioc_.run(); 
                LOG_DEBUG("IO service thread terminated");
            });
        }
        
        LOG_INFO("WebSocket server running with %zu threads", server_threads_.size());
        
    } catch (const std::exception& e) {
        LOG_CRITICAL("Failed to start WebSocket server: %s", e.what());
        throw;
    }
}

void WebsocketServer::do_accept() {
    LOG_DEBUG("Setting up async accept");
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            on_accept(ec, std::move(socket));
        });
}

void WebsocketServer::on_accept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
    if(ec) {
        LOG_ERROR("Accept error: %s", ec.message().c_str());
    } else {
        std::string client_endpoint = socket.remote_endpoint().address().to_string() + 
                                     ":" + std::to_string(socket.remote_endpoint().port());
        LOG_INFO("New connection from %s", client_endpoint.c_str());
        
        auto session = std::make_shared<WebSocketSession>(
            std::move(socket),
            [this](std::shared_ptr<WebSocketSession> session, const std::string& message) {
                handle_client_message(session, message);
            });
            
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_.push_back(session);
            subscriptions_[session] = std::set<std::string>();
            LOG_DEBUG("Added new session to sessions list, total sessions: %zu", sessions_.size());
        }
        
        session->start();
        
        LOG_INFO("New client session started");
    }
    
    if(running_) {
        do_accept();
    }
}

void WebsocketServer::handle_client_message(std::shared_ptr<WebSocketSession> session, const std::string& message) {
    try {
        LOG_INFO("Received message from client: %s", message.c_str());
        
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(message, json)) {
            LOG_WARNING("Failed to parse JSON message from client");
            return;
        }
        
        std::string action = json["action"].asString();
        std::string symbol = json["symbol"].asString();
        
        if (action == "subscribe") {
            LOG_INFO("Client subscribing to symbol: %s", symbol.c_str());
            
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                subscriptions_[session].insert(symbol);
                LOG_DEBUG("Added symbol %s to client's subscriptions", symbol.c_str());
            }
            
            subscribe_to_orderbook(symbol);
            
        } else if (action == "unsubscribe") {
            LOG_INFO("Client unsubscribing from symbol: %s", symbol.c_str());
            
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                subscriptions_[session].erase(symbol);
                LOG_DEBUG("Removed symbol %s from client's subscriptions", symbol.c_str());
            }
        } else {
            LOG_WARNING("Unknown action in client message: %s", action.c_str());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error processing client message: %s", e.what());
    }
}

void WebsocketServer::subscribe_to_orderbook(const std::string& symbol) {
    if (!deribit_connected_) {
        LOG_WARNING("Cannot subscribe to %s: No connection to Deribit", symbol.c_str());
        return;
    }

    LOG_INFO("Subscribing to orderbook for %s", symbol.c_str());
    
    Json::Value subscription;
    subscription["jsonrpc"] = "2.0";
    subscription["id"] = 42;
    subscription["method"] = "public/subscribe";
    subscription["params"]["channels"] = Json::arrayValue;
    subscription["params"]["channels"].append("book." + symbol + ".100ms");

    std::string message = Json::FastWriter().write(subscription);
    LOG_DEBUG("Sending subscription request to Deribit: %s", message.c_str());
    
    try {
        boost::beast::flat_buffer buffer;
        deribit_ws_->write(boost::asio::buffer(message));
        LOG_INFO("Successfully subscribed to orderbook for %s", symbol.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR("Error subscribing to orderbook: %s", e.what());
    }
}

void WebsocketServer::init_deribit_connection() {
    try {
        LOG_INFO("Initializing connection to Deribit");
        deribit_ioc_ = std::make_unique<boost::asio::io_context>();
        
        boost::asio::ip::tcp::resolver resolver(*deribit_ioc_);
        
        std::string host = config_.WS_URL;
        std::string port = "443";
        
        if (host.substr(0, 6) == "wss://") {
            host = host.substr(6);
        } else if (host.substr(0, 5) == "ws://") {
            host = host.substr(5);
            port = "80";
        }
        
        auto pos = host.find(':');
        if (pos != std::string::npos) {
            port = host.substr(pos + 1);
            host = host.substr(0, pos);
        }
        
        LOG_INFO("Resolving Deribit host: %s:%s", host.c_str(), port.c_str());
        auto const results = resolver.resolve(host, port);
        
        auto socket = boost::asio::ip::tcp::socket(*deribit_ioc_);
        boost::asio::connect(socket, results.begin(), results.end());
        LOG_DEBUG("TCP connection established to Deribit");
        
        auto ssl_stream = boost::beast::ssl_stream<boost::asio::ip::tcp::socket>(
            std::move(socket), ssl_ctx_);
            
        if(!SSL_set_tlsext_host_name(ssl_stream.native_handle(), host.c_str())) {
            boost::system::error_code ec{static_cast<int>(::ERR_get_error()), 
                                         boost::asio::error::get_ssl_category()};
            LOG_ERROR("SSL SNI error: %s", ec.message().c_str());
            throw boost::system::system_error{ec};
        }
        
        LOG_DEBUG("Performing SSL handshake with Deribit");
        ssl_stream.handshake(boost::asio::ssl::stream_base::client);
        LOG_DEBUG("SSL handshake successful");
        
        deribit_ws_ = std::make_unique<boost::beast::websocket::stream<
                            boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>>(
                                std::move(ssl_stream));
                                
        deribit_ws_->set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::request_type& req) {
                req.set(boost::beast::http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " deribit-trading-client");
            }));
            
        LOG_DEBUG("Performing WebSocket handshake with Deribit");
        deribit_ws_->handshake(host, "/ws/api/v2");
        LOG_INFO("Successfully connected to Deribit WebSocket");
        
        deribit_connected_ = true;
        
        deribit_thread_ = std::make_unique<std::thread>([this]() {
            LOG_INFO("Deribit message reader thread started");
            try {
                while (deribit_connected_) {
                    boost::beast::flat_buffer buffer;
                    LOG_DEBUG("Waiting for message from Deribit");
                    deribit_ws_->read(buffer);
                    
                    std::string payload = boost::beast::buffers_to_string(buffer.data());
                    LOG_DEBUG("Received %zu bytes from Deribit", payload.size());
                    on_deribit_message(payload);
                }
            } catch (const boost::beast::system_error& e) {
                if (e.code() == boost::beast::websocket::error::closed) {
                    LOG_INFO("Deribit WebSocket connection closed");
                } else {
                    LOG_ERROR("Deribit WebSocket error: %s", e.what());
                }
                deribit_connected_ = false;
            } catch (const std::exception& e) {
                LOG_ERROR("Deribit WebSocket error: %s", e.what());
                deribit_connected_ = false;
            }
            LOG_INFO("Deribit message reader thread terminated");
        });
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error initializing Deribit connection: %s", e.what());
    }
}

void WebsocketServer::on_deribit_message(const std::string& payload) {
    try {
        LOG_DEBUG("Processing message from Deribit: %s", payload.c_str());
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(payload, root)) {
            LOG_WARNING("Failed to parse JSON message from Deribit");
            return;
        }
        
        if (root.isMember("params") && root["params"].isMember("channel")) {
            std::string channel = root["params"]["channel"].asString();
            size_t firstDot = channel.find('.');
            size_t secondDot = channel.find('.', firstDot + 1);
            if (firstDot != std::string::npos && secondDot != std::string::npos) {
                std::string symbol = channel.substr(firstDot + 1, secondDot - firstDot - 1);
                LOG_INFO("Received orderbook update for %s", symbol.c_str());
                handle_orderbook_update(symbol, payload);
            } else {
                LOG_WARNING("Received message with unexpected channel format: %s", channel.c_str());
            }
        } else if (root.isMember("id") && root.isMember("result")) {
            LOG_INFO("Received response to request with id: %s", root["id"].asString().c_str());
        } else {
            LOG_WARNING("Received message with unexpected format");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error processing Deribit message: %s", e.what());
    }
}

void WebsocketServer::handle_orderbook_update(const std::string& symbol, const std::string& data) {
    LOG_DEBUG("Handling orderbook update for %s", symbol.c_str());
    auto start_time = std::chrono::high_resolution_clock::now();
    
    broadcast_to_subscribers(symbol, data);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    LOG_INFO("Message propagation time for %s: %lld microseconds", symbol.c_str(), duration.count());
}

void WebsocketServer::broadcast_to_subscribers(const std::string& symbol, const std::string& data) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    std::vector<std::shared_ptr<WebSocketSession>> recipients;
    
    for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
        if (it->second.find(symbol) != it->second.end()) {
            recipients.push_back(it->first);
        }
    }
    
    LOG_DEBUG("Broadcasting %s update to %zu subscribers", symbol.c_str(), recipients.size());
    
    for (auto& session : recipients) {
        session->send(data);
    }
}

void WebsocketServer::stop() {
    try {
        LOG_INFO("Stopping WebSocket server...");
        
        running_ = false;
        
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            LOG_DEBUG("Closing %zu active WebSocket sessions", sessions_.size());
            for (auto& session : sessions_) {
                session->close();
            }
            sessions_.clear();
            subscriptions_.clear();
        }
        
        boost::system::error_code ec;
        acceptor_.close(ec);
        if (ec) {
            LOG_WARNING("Error closing acceptor: %s", ec.message().c_str());
        }
        
        LOG_DEBUG("Stopping IO context");
        ioc_.stop();
        
        LOG_DEBUG("Joining %zu server threads", server_threads_.size());
        for (auto& t : server_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
        server_threads_.clear();
        
        LOG_INFO("Stopping Deribit WebSocket client...");
        
        if (deribit_connected_) {
            deribit_connected_ = false;
            
            if (deribit_ws_) {
                LOG_DEBUG("Gracefully closing Deribit WebSocket connection");
                boost::beast::websocket::close_reason reason;
                reason.code = boost::beast::websocket::close_code::normal;
                reason.reason = "Client shutting down";
                
                boost::system::error_code ec;
                deribit_ws_->close(reason, ec);
                if (ec) {
                    LOG_WARNING("Error closing Deribit WebSocket: %s", ec.message().c_str());
                }
                deribit_ws_.reset();
            }
            
            if (deribit_ioc_) {
                LOG_DEBUG("Stopping Deribit IO context");
                deribit_ioc_->stop();
            }
            
            if (deribit_thread_ && deribit_thread_->joinable()) {
                LOG_DEBUG("Joining Deribit thread");
                deribit_thread_->join();
            }
            deribit_thread_.reset();
        }
        
        LOG_INFO("WebSocket server and Deribit client stopped successfully");
    } catch (const std::exception& e) {
        LOG_CRITICAL("Error stopping WebSocket server or Deribit client: %s", e.what());
    }
}

} // namespace deribit