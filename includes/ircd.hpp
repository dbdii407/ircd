#pragma once

#include "tools.hpp"
#include "net.hpp"

namespace ircd {
  enum class event {
    RECIEVED,
    SENT
  };

  enum class state {
    DISCONNECTED,
    CONNECTED,
    LISTENING,
    ERROR
  };

  class Socket : public ee::Emitter {
    friend class Server;

    private:
      std::string buffer;
      net::Socket* sock;

    protected:
      Socket(net::Socket* in) : sock(in) {
        setup();
      }

      void setup() {
        sock->on(net::state::DISCONNECTED, [&]() {
          tools::log("socket closed");

          emit(state::DISCONNECTED);
        });

        sock->on(net::state::CONNECTED, [&]() {
          emit(state::CONNECTED);
        });

        sock->on(net::event::RECIEVED, [&](std::string recvd) {
          buffer += recvd;

          if (!buffer.ends_with("\n"))
            return;

          tools::split(buffer, "\n", [&](std::string line) {
            emit(event::RECIEVED, line);
          });

          buffer.clear();
        });

        sock->on(net::event::SENT, [&](std::string out) {
          emit(event::SENT, out);
        });
      }

    public:
      Socket() {
        sock = new net::Socket();
        setup();
      }

      std::string addr() const {
        return sock->addr();
      }

      uint16_t port() const {
        return sock->port();
      }

      template <typename ...A> void out(std::string form, A ...args) {
        if (!form.ends_with("\r\n"))
          form += "\r\n";
          
        sock->out(form, args ...);
      }

      void connect(uint16_t port, std::string host) {
        sock->connect(port, host);
      }
  };

  class Server : public ee::Emitter {
    private:
      net::Server* serv;
      char** argv;
      int argc;

    public:
      Server(int argc, char* argv[]) : argc(argc), argv(argv) {
        serv = new net::Server();

        serv->on(net::state::LISTENING, [&]() {
          emit(state::LISTENING);
        });

        serv->on(net::state::CONNECTED, [&](net::Socket* inc) {
          auto in = new Socket(inc);

          emit(state::CONNECTED, in);
        });
      }

      void listen(uint16_t port, std::string host = "localhost") {
        serv->listen(port, host);
      }
  };
}
