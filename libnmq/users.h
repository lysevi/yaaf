#pragma once

#include <libnmq/utils/utils.h>
#include <cstddef>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace nmq {

using Id = uint64_t;
struct User {
  std::string login;
  Id id;
};

static const int ServerID = std::numeric_limits<int>::max();

class UserBase;
using UserBase_Ptr = std::shared_ptr<UserBase>;
class UserBase {
public:
  static UserBase_Ptr create() { return std::make_shared<UserBase>(); }

  void append(const User &user) {
    std::lock_guard<std::shared_mutex> sl(_locker);
    logger_info("node: add client #", user.id);
    _users[user.id] = user;
  }

  void setLogin(Id id, const std::string &login) {
    std::lock_guard<std::shared_mutex> sl(_locker);
    logger_info("node: set login id=#", id, " - '", login, "'");
    _users[id].login = login;
  }

  void erase(Id id) {
    std::lock_guard<std::shared_mutex> sl(_locker);
    auto it = _users.find(id);
    if (it != _users.end()) {
      logger_info("node: client id=#", id, ", login:", it->second.login, " erased");
      _users.erase(it);
    } else {
      THROW_EXCEPTION("node: user id=#", id, "not exists");
    }
  }

  bool exists(Id id) const {
    std::shared_lock<std::shared_mutex> sl(_locker);
    auto it = _users.find(id);
    return it != _users.end();
  }

  bool byId(Id id, User &output) const {
    std::shared_lock<std::shared_mutex> sl(_locker);
    auto it = _users.find(id);
    if (it == _users.end()) {
      return false;
    } else {
      output = it->second;
      return true;
    }
  }

  std::vector<User> users() const {
    std::shared_lock<std::shared_mutex> sl(_locker);
    std::vector<User> result(_users.size());
    auto it = result.begin();
    for (auto kv : _users) {
      *it = kv.second;
      ++it;
    }
    return result;
  }

protected:
  mutable std::shared_mutex _locker;
  std::unordered_map<Id, User> _users;
};
} // namespace nmq
