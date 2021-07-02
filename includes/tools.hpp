#pragma once

#include <string>

namespace tools {
  template <typename ...A>
    std::string format(std::string str, A ...args) {
      auto length = std::snprintf(nullptr, 0, &str[0], args...);
      auto out = std::string(length, '\0');
      std::sprintf(&out[0], &str[0], args...);
      return out.data();
    }

  template <typename ...A>
    void log(std::string str, A ...args) {
      auto text = format(str, args ...);

      printf(&text[0]);

      if (!text.ends_with("\n"))
        printf("\n");
    }
    
  template <typename LF>
    void split(std::string str, std::string delm, LF lfunc) {
      for (auto i = str.find_first_of(delm); i != EOF; i = str.find_first_of(delm)) {
        auto next = str.substr(0, i);
        str.erase(0, i + delm.size());
        lfunc(next);
      }

      if (!str.empty())
        lfunc(str);
    }

  template <template <typename, typename ...> typename C, typename T, typename ...A>
    void remove(C<T, A...> &cont, T find) {
      for (auto iter = cont.begin(); iter != cont.end(); ++iter) {
        if (*iter != find)
          continue;

        iter = --cont.erase(iter);
        return;
      }
    }


  template <template <typename, typename ...> typename C, typename T, typename ...A, typename LF>
    void remove_if(C<T, A...> &cont, LF lfunc) {
      for (auto iter = cont.begin(); iter != cont.end(); ++iter) {
        if (!lfunc(*iter))
          continue;

        iter = --cont.erase(iter);
      }
    }
}
