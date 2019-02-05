#pragma once

#include <libnmq/exports.h>
#include <atomic>
#include <functional>
#include <memory>

namespace nmq {

struct envelope;
class mailbox;
class actor;

using actor_ptr = std::shared_ptr<actor>;
using actor_weak = std::weak_ptr<actor>;

enum class actor_status_kinds { NORMAL, WITH_ERROR };

class actor : public std::enable_shared_from_this<actor> {
public:
  using delegate_t = std::function<void(actor_weak, const envelope &)>;

  struct status_t {
    actor_status_kinds kind;
    std::string msg;
  };

  actor(const actor &a) = delete;
  actor() = delete;
  actor(actor &&a) = delete;

  actor(delegate_t callback) : _handle(callback), _busy(false) {
    _status.kind = actor_status_kinds::NORMAL;
  }
  ~actor() {}

  EXPORT void apply(mailbox &mbox);
  bool busy() const { return _busy.load(); }
  status_t status() const { return _status; }

private:
  mutable std::atomic_bool _busy;
  delegate_t _handle;

  status_t _status;
};

} // namespace nmq