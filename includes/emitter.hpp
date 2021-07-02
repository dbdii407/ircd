#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <any>

namespace ee {
  class Emitter {
    private:
      struct listener {
        public:
          explicit listener() = default;
          listener(const listener &) = delete;
          listener &operator=(const listener &) = delete;
          virtual ~listener() {}
      };

      template <typename ...A> struct handler : public listener {
        public:
          handler(std::function<void(A ...)> func) : func(func) { }
          std::function<void(A ...)> func;
      };

      struct eevent {
        public:
          std::unique_ptr<listener> handle;
          std::any ident;
          bool once;

          template <typename ...A> void call(A ...args) {
            auto handlr = static_cast<handler<A ...>*>(handle.get());

            try { handlr->func(args ...); }
            catch (...) { }
          }

          template <typename T> bool is(T ident) {
            if (this->ident.type() != typeid(T))
              return !1;

            T id = std::any_cast<T>(this->ident);

            return id == ident;
          }
      };

      std::vector<eevent*> list;

      template <typename T, typename ...A>
        void add(T ident, bool once, std::function<void(A ...)> func) {
          auto wrap = new handler(func);

          auto evnt = new eevent({
            handle: std::unique_ptr<listener>(wrap),
            ident: std::any(ident),
            once: once
          });

          list.push_back(evnt);
        }

    public:
      template <typename T, typename LF> void once(T ident, LF lfunc) {
        add(ident, !0, std::function(lfunc));
      }

      template <typename T, typename LF> void on(T ident, LF lfunc) {
        add(ident, !1, std::function(lfunc));
      }

      template <typename T, typename ...A> void emit(T ident, A ...args) {
        auto end = std::end(list);

        for (auto iter = std::begin(list); iter != end; iter++) {
          if (!(*iter)->is(ident))
            continue;

          (*iter)->call(args ...);

          if ((*iter)->once)
            list.erase(iter);
        }
      }
  };
}
