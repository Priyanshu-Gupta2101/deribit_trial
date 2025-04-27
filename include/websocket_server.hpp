#pragma once

#include "config.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <json/json.h>
#include <map>
#include <set>
#include <memory>
#include <thread>
#include <mutex>
#include <functional>

namespace deribit {

class WebSocketSession;

class WebsocketServer {
public:
    explicit WebsocketServer(Config& config);
    ~WebsocketServer();

    void run(uint16_t port);
    void stop();

private:
    void do_accept();
    void on_accept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket);
    void handle_client_message(std::shared_ptr<WebSocketSession> session, const std::string& message);
    void subscribe_to_orderbook(const std::string& symbol);
    void handle_orderbook_update(const std::string& symbol, const std::string& data);
    void init_deribit_connection();
    void on_deribit_message(const std::string& message);
    void broadcast_to_subscribers(const std::string& symbol, const std::string& data);

    Config& config_;
    boost::asio::io_context ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::thread> server_threads_;
    std::atomic<bool> running_;
    
    std::mutex sessions_mutex_;
    std::vector<std::shared_ptr<WebSocketSession>> sessions_;
    std::map<std::shared_ptr<WebSocketSession>, std::set<std::string>> subscriptions_;
    
    std::unique_ptr<boost::asio::io_context> deribit_ioc_;
    std::unique_ptr<boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>> deribit_ws_;
    std::unique_ptr<std::thread> deribit_thread_;
    std::atomic<bool> deribit_connected_;
    boost::asio::ssl::context ssl_ctx_;
};

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    using message_handler = std::function<void(std::shared_ptr<WebSocketSession>, const std::string&)>;

    explicit WebSocketSession(
        boost::asio::ip::tcp::socket socket,
        message_handler on_message
    );

    void start();
    void send(const std::string& message);
    void close();

private:
    void do_read();
    void on_read(boost::system::error_code ec, std::size_t bytes_transferred);
    void on_write(boost::system::error_code ec, std::size_t bytes_transferred);

    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
    boost::beast::flat_buffer buffer_;
    message_handler on_message_;
    std::mutex write_mutex_;
};

} // namespace deribit