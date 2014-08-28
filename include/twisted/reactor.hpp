#ifndef TWISTEDCPP_REACTOR_HPP
#define TWISTEDCPP_REACTOR_HPP

#include "detail/sockets.hpp"
#include "ssl_options.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/system_error.hpp>

#include <memory>
#include <thread>

namespace twisted {

class reactor {
public:
    void run(int thread_count = 1) {
        for (int i = 0; i != thread_count - 1; ++i) {
            worker_threads.emplace_back([&] { _io_service.run(); });
        }

        _io_service.run();
    }

    void stop() {
        _io_service.stop();

        for (auto&& t : worker_threads) {
            t.join();
        }
    }

    template <typename ProtocolFactory>
    void listen_tcp(int port, ProtocolFactory factory) {
        boost::asio::spawn(_io_service, [=](boost::asio::yield_context yield) {
            using boost::asio::ip::tcp;
            tcp::acceptor acceptor(_io_service, tcp::endpoint(tcp::v4(), port));
            tcp_core(acceptor, factory, yield);
        });
    }

    template <typename ProtocolFactory>
    void listen_ssl(int port, ProtocolFactory factory, ssl_options&& ssl_opts) {
        std::shared_ptr<ssl_options> ssl_opts_ptr // move capture
            = std::make_shared<ssl_options>(std::move(ssl_opts));
        boost::asio::spawn(_io_service, [=](boost::asio::yield_context yield) {
            using boost::asio::ip::tcp;
            tcp::acceptor acceptor(_io_service, tcp::endpoint(tcp::v4(), port));
            ssl_impl(acceptor, factory, yield, ssl_opts_ptr);
        });
    }

private:
    template <typename ProtocolFactory>
    void tcp_core(boost::asio::ip::tcp::acceptor& acceptor,
                  ProtocolFactory factory, boost::asio::yield_context yield) {
        auto socket_factory = [=]() {
            return std::unique_ptr<detail::tcp_socket>(
                new detail::tcp_socket(_io_service));
        };

        run_loop(acceptor, std::move(factory), yield, socket_factory);
    }

    template <typename ProtocolFactory>
    void ssl_impl(boost::asio::ip::tcp::acceptor& acceptor,
                  ProtocolFactory factory, boost::asio::yield_context yield,
                  std::shared_ptr<ssl_options> ssl_context) {
        auto socket_factory = [&] {
            return std::unique_ptr<detail::ssl_socket>(
                new detail::ssl_socket(_io_service, *ssl_context));
        };

        run_loop(acceptor, std::move(factory), yield, socket_factory);
    }

    template <typename ProtocolFactory, typename SocketFactory>
    void run_loop(boost::asio::ip::tcp::acceptor& acceptor,
                  ProtocolFactory factory, boost::asio::yield_context yield,
                  SocketFactory socket_factory) {
        while (true) {
            auto socket = socket_factory();

            acceptor.async_accept(socket->lowest_layer(), yield);

            auto new_client = std::make_shared<decltype(factory())>(factory());
            // lazy init to avoid clutter in protocol constructors
            new_client->set_socket(std::move(socket));
            new_client->run_protocol();
        }
    }

    boost::asio::io_service _io_service;
    std::vector<std::thread> worker_threads;
};

} // namespace twisted

#endif
