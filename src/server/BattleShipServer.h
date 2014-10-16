#ifndef _BATTLESHIPSERVER_H
#define _BATTLESHIPSERVER_H

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/bind.hpp>

#include <list>
#include <memory>

#include "Connection.h"

namespace asio = boost::asio;

class BattleShipServer {
    private:
        asio::io_service io_service;
        asio::ip::tcp::socket socket;
        asio::ip::tcp::endpoint endpoint;
        asio::ip::tcp::acceptor acceptor;

        std::list<std::unique_ptr<Connection>> connections;

    public:
        BattleShipServer();

        void handle_io();

        std::list<std::unique_ptr<Connection>>& get_connections();

    private:
        void accept_connections();
};
#endif
