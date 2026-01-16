#include "async/async.h"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <shared_mutex>

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
private:
    tcp::socket socket_;
    std::array<char, 8192> buffer_;
    std::string leftover_;
    async::handle_t context_;
    
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    // Обрабатываем полученные данные
                    async::receive(context_, buffer_.data(), length);
                    
                    // Продолжаем чтение
                    do_read();
                } else if (ec != boost::asio::error::operation_aborted) {
                    // Соединение закрыто
                    async::disconnect(context_);
                }
            });
    }

public:
    Session(tcp::socket socket, std::size_t bulk_size)
        : socket_(std::move(socket)) {
        context_ = async::connect(bulk_size);
    }

    ~Session() {
        async::disconnect(context_);
    }

    void start() {
        do_read();
    }
};

class Server {
private:
    boost::asio::io_context io_context_;
    tcp::acceptor acceptor_;
    std::size_t bulk_size_;
    std::vector<std::thread> worker_threads_;
    
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), bulk_size_)->start();
                }
                do_accept();
            });
    }

public:
    Server(unsigned short port, std::size_t bulk_size, std::size_t thread_pool_size = 1)
        : acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)),
          bulk_size_(bulk_size) {
        do_accept();
        
        // Запускаем пул потоков для обработки соединений
        for (std::size_t i = 0; i < thread_pool_size; ++i) {
            worker_threads_.emplace_back([this]() {
                io_context_.run();
            });
        }
    }

    ~Server() {
        io_context_.stop();
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void run() {
        // Блокируем основной поток
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: bulk_server <port> <bulk_size>\n";
        return 1;
    }

    try {
        unsigned short port = std::stoi(argv[1]);
        std::size_t bulk_size = std::stoul(argv[2]);
        
        std::cout << "Starting bulk server on port " << port 
                  << " with bulk size " << bulk_size << std::endl;
        
        Server server(port, bulk_size, std::thread::hardware_concurrency());
        server.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}