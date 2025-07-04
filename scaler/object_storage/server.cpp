#include "server.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/system/system_error.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "constants.h"
#include "object_storage_server.h"
#include "version.h"

// Helper macros to stringify the macro value
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x)        STRINGIFY_HELPER(x)

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
namespace this_coro = boost::asio::this_coro;

#if defined(BOOST_ASIO_ENABLE_HANDLER_TRACKING)
#define use_awaitable boost::asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

scaler::object_storage::ObjectStorageServer server;

awaitable<void> listener(boost::asio::ip::tcp::endpoint endpoint) {
    auto executor = co_await this_coro::executor;
    tcp::acceptor acceptor(executor, endpoint);
    for (;;) {
        tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
        co_spawn(executor, server.process_request(std::move(socket)), detached);
    }
}

// Assuming name_ and port_ is valid name and port
void run_object_storage_server(const char* name_, const char* port_) {
    std::string name = name_;
    std::string port = port_;

    try {
        boost::asio::io_context io_context(1);
        tcp::resolver resolver(io_context);
        auto res = resolver.resolve(name, port);

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        co_spawn(io_context, listener(res.begin()->endpoint()), detached);

        io_context.run();
    } catch (std::exception& e) { std::printf("Exception: %s\n", e.what()); }
}
