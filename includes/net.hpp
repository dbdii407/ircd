#pragma once

#include <arpa/inet.h>
#include <stdexcept>
#include <netdb.h>
#include <variant>
#include <thread>
#include <list>

#include "emitter.hpp"

namespace net {
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

  enum class type {
    Unknown = -1,
    IPv6,
    IPv4
  };

  std::list<addrinfo*> lookup(std::string domain) {
    auto res = new addrinfo();

    auto hints = addrinfo({
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
      .ai_protocol = IPPROTO_TCP
    });

    auto r = getaddrinfo(&domain[0], NULL, &hints, &res);

    if (r != EXIT_SUCCESS)
      throw new std::runtime_error("getaddrinfo failed");

    auto list = std::list<addrinfo*>();

    for (auto next = res; next != NULL; next = next->ai_next)
      list.push_back(next);

    return list;
  }

  type type_of(std::string ip) {
    auto sa = new sockaddr();

    if (inet_pton(AF_INET6, &ip[0], &((sockaddr_in6 *) sa)->sin6_addr) != 0)
      return type::IPv6;

    if (inet_pton(AF_INET, &ip[0], &((sockaddr_in *) sa)->sin_addr) != 0)
      return type::IPv4;

    return type::Unknown;    
  }

  void set_addr(addrinfo* &ai, std::string ip) { // assign an ip to an addrinfo struct
    if (ai->ai_family != AF_UNSPEC)
      throw std::invalid_argument("addrinfo* already has an ip address");

    auto ipt = type_of(ip);

    switch (ipt) {
      case type::IPv6: {
        ai->ai_addr = (sockaddr *) new sockaddr_in6();
        ai->ai_addrlen = INET6_ADDRSTRLEN;
        ai->ai_family = AF_INET6;

        auto sa = ((sockaddr_in6 *) ai->ai_addr);
        inet_pton(AF_INET6, &ip[0], &sa->sin6_addr);
        sa->sin6_family = AF_INET6; // bind
        break;
      }

      case type::IPv4: {
        ai->ai_addr = (sockaddr *) new sockaddr_in();
        ai->ai_addrlen = INET_ADDRSTRLEN;
        ai->ai_family = AF_INET;

        auto sa = ((sockaddr_in *) ai->ai_addr);
        inet_pton(AF_INET, &ip[0], &sa->sin_addr);
        sa->sin_family = AF_INET; // bind
        break;
      }

      case type::Unknown:
        throw std::invalid_argument("recieved invalid ip address");
    }
  }

  std::string get_addr(addrinfo* ai) { // return the ip of an addrinfo struct, if assigned
    auto out = std::string();

    switch (ai->ai_family) {
      case AF_INET6: {
        auto ia = &((sockaddr_in6 *) ai->ai_addr)->sin6_addr;
        inet_ntop(AF_INET6, ia, &out[0], ai->ai_addrlen);
        break;
      }

      case AF_INET: {
        auto ia = ((sockaddr_in *) ai->ai_addr)->sin_addr;
        out = (std::string) inet_ntoa(ia);
        break;
      }
    }

    return out;
  }

  void set_port(addrinfo* &ai, uint16_t port) { // assign a port to an unspecified addrinfo struct
    switch (ai->ai_family) {
      case AF_INET6:
        ((sockaddr_in6 *) ai->ai_addr)->sin6_port = htons(port);
        break;

      case AF_INET:
        ((sockaddr_in *) ai->ai_addr)->sin_port = htons(port);
        break;

      default:
        throw std::invalid_argument("addrinfo* family is invalid");
    }
  }

  uint16_t get_port(addrinfo* ai) { // return the port of an addrinfo struct, if assigned
    auto out = uint(0);

    switch (ai->ai_family) {
      case AF_INET6:
        out = ((sockaddr_in6 *) ai->ai_addr)->sin6_port;
        break;

      case AF_INET:
        out = ((sockaddr_in *) ai->ai_addr)->sin_port;
        break;
    }

    return (int) ntohs(out);
  }

  bool enable_opt(int id, uint type) {
    const int opt = 1;

    auto i = ::setsockopt(id, SOL_SOCKET, type, &opt, sizeof(int));

    return (i == EOF) ? !1 : !0;
  }

  bool disable_opt(int id, uint type) {
    const int opt = 0;

    auto i = ::setsockopt(id, SOL_SOCKET, type, &opt, sizeof(int));

    return (i == EOF) ? !1 : !0;
  }

  std::variant<state, std::string> recv(int id, int size = 1024) {
    auto buffer = std::string();
    char recvd[size];

    while (!0) {
      auto i = ::recv(id, &recvd, size, 0);

      if (i == EOF) {
        if (errno == EAGAIN)
          continue;

        return state::ERROR;
      }

      if (i == EXIT_SUCCESS)
        return state::DISCONNECTED;

      buffer.append(recvd, i);

      if (i < size)
        return buffer;
    }
  }

  template <typename ...A>
    std::string send(int id, std::string str, A ...args) {
      auto length = std::snprintf(nullptr, 0, &str[0], args...);
      auto out = std::string(length, '\0');

      std::sprintf(&out[0], &str[0], args...);

      auto offset = 0;

      while (!0) {
        offset += ::send(id, &(out.substr(offset))[0], out.size() - offset, 0);

        if (offset >= out.length())
          return out;
      }
    }

  std::pair<int, addrinfo*> accept(uint id, addrinfo* ai) {
    auto ca = new addrinfo({
      .ai_family = ai->ai_family,
      .ai_addrlen = ai->ai_addrlen
    });

    switch (ai->ai_family) {
      case AF_INET6:
        ca->ai_addr = (sockaddr *) new sockaddr_in6();
        break;

      case AF_INET:
        ca->ai_addr = (sockaddr *) new sockaddr_in();
        break;

      default:
        throw std::invalid_argument("net::accept, addrinfo* has invalid family");
    }

    auto i = ::accept(id, ca->ai_addr, &ca->ai_addrlen);

    if (i == EOF)
      throw std::runtime_error("unable to accept incoming");

    return std::pair(i, ca);
  }

  bool connect(int id, addrinfo* ai) {
    return ::connect(id, ai->ai_addr, ai->ai_addrlen) != EOF
      ? !0 : !1;
  }

  bool bind(int id, addrinfo* ai) {
    return ::bind(id, ai->ai_addr, ai->ai_addrlen) != EOF
      ? !0 : !1;
  }

  bool close(int id) {
    return ::shutdown(id, SHUT_RDWR) == EXIT_SUCCESS;
  }

  std::pair<int, addrinfo*> setup(uint16_t port, std::string host, std::initializer_list<int> opts) {
    for (auto next : lookup(host)) {
      auto id = ::socket(next->ai_family, next->ai_socktype, 0);

      if (id == EOF)
        continue;

      for (auto next : opts)
        if (!enable_opt(id, next))
          continue;

      set_port(next, port);

      return std::pair(id, next);
    }

    throw std::runtime_error("net, unable to setup socket");
  }

  class Socket : public ee::Emitter {
    friend class Server;
    
    private:
      std::thread thread;
      addrinfo* ai;
      bool linked;
      int id;

    protected:
      using ee::Emitter::emit;

      Socket(int id, addrinfo* ai) : id(id), ai(ai) {
        read();
      }

      void read() {
        if (linked)
          return;

        linked = !0;

        thread = std::thread([&]() {
          while (!0) {
            auto vari = recv(id);

            if (std::holds_alternative<enum state>(vari)) {
              linked = !1;
              return emit(state::DISCONNECTED);
            }

            else {
              auto recvd = std::get<std::string>(vari);
              emit(event::RECIEVED, recvd);
            }
          }
        });

        thread.detach();
      }

    public:
      Socket() {

      }
  
      template <typename ...A> void out(std::string form, A ...args) {
        auto out = send(id, form, args ...);

        emit(event::SENT, out);
      }

      std::string addr() const {
        return get_addr(ai);
      }

      uint16_t port() const {
        return get_port(ai);
      }

      bool connected() const {
        return linked;
      }

      void connect(uint16_t port, std::string host) {
        auto [sid, sai] = setup(port, host, {
          SO_REUSEADDR,
          SO_REUSEPORT
        });

        auto i = net::connect(sid, sai);

        if (!i)
          throw std::runtime_error("net, unable to connect socket");

        ai = sai;
        id = sid;

        emit(state::CONNECTED);

        read();
      }
  };

  class Server : public ee::Emitter {
    private:
      std::thread thread;

    public:
      Server() {

      }

      void listen(uint16_t port, std::string host = "localhost") {
        auto [id, ai] = setup(port, host, {
          SO_REUSEADDR,
          SO_REUSEPORT
        });

        auto i = bind(id, ai);

        if (!i)
          throw std::runtime_error("net, unable to bind server");

        emit(state::LISTENING);
          
        thread = std::thread([this, id, ai]() {
          while (!0) {
            auto i = ::listen(id, 1);

            if (i == EOF)
              return;

            auto [ci, ca] = accept(id, ai);

            auto inc = new Socket(ci, ca);

            emit(state::CONNECTED, inc);
          }
        });

        thread.detach();
    }
  };
}
