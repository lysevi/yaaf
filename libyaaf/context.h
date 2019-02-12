#pragma once

#include <libyaaf/abstract_context.h>
#include <libyaaf/actor.h>
#include <libyaaf/exports.h>
#include <libyaaf/types.h>
#include <libyaaf/utils/async/thread_manager.h>

#include <memory>
#include <shared_mutex>
#include <unordered_set>

namespace yaaf {
namespace inner {

struct description {
  actor_ptr actor;
  actor_address address;
  actor_settings settings;
  std::shared_ptr<abstract_context> usrcont;
  std::string name;
  id_t parent;
  std::unordered_set<id_t> children;
};
} // namespace inner

using yaaf::utils::async::CONTINUATION_STRATEGY;
using yaaf::utils::async::task_result_ptr;

class context : public abstract_context, public std::enable_shared_from_this<context> {
public:
  using abstract_context::add_actor;
  using abstract_context::get_actor;
  using abstract_context::make_actor;
  using abstract_context::send;

  struct params_t {
    EXPORT static params_t defparams();

    size_t user_threads;
    size_t sys_threads;
  };

  EXPORT static std::shared_ptr<context> make_context(std::string name = "");
  EXPORT static std::shared_ptr<context> make_context(const params_t &params,
                                                      std::string name = "");

  EXPORT context(const params_t &p, std::string name = "");
  EXPORT ~context();
  EXPORT void start();
  EXPORT void stop();

  EXPORT void send_envelope(const actor_address &target, const envelope &e) override;
  EXPORT void send_envelope(const actor_address &target, const envelope &&e) override;
  EXPORT actor_address add_actor(const std::string &actor_name,
                                 const actor_ptr a) override;
  EXPORT actor_address add_actor(const std::string &actor_name,
                                 const actor_address &parent, const actor_ptr a);
  EXPORT void stop_actor(const actor_address &addr) override;
  EXPORT actor_weak get_actor(const actor_address &addr) const override;
  EXPORT actor_weak get_actor(const std::string &name) const override;
  EXPORT std::string name() const override;

  bool is_stopping_begin() const { return _stopping_begin; }

private:
  void mailbox_worker();
  void stop_actor_impl_safety(const actor_address &addr, actor_stopping_reason reason);
  void stop_actor_impl(const actor_address &addr, actor_stopping_reason reason);

  task_result_ptr
  user_post(const std::function<void()> &f,
            CONTINUATION_STRATEGY strategy = CONTINUATION_STRATEGY::SINGLE);

  task_result_ptr
  sys_post(const std::function<void()> &f,
           CONTINUATION_STRATEGY strategy = CONTINUATION_STRATEGY::SINGLE);

  void apply_actor_to_mailbox(
      const std::shared_ptr<inner::description> target_actor_description,
      actor_ptr parent, std::shared_ptr<mailbox> mb);

  void on_actor_error(actor_action_when_error action,
                      const std::shared_ptr<inner::description> target_actor_description,
                      actor_ptr parent);

private:
  params_t _params;
  std::string _name;
  std::unique_ptr<utils::async::thread_manager> _thread_manager;

  mutable std::shared_mutex _locker;
  std::atomic_uint64_t _next_actor_id{1};

  std::unordered_map<id_t, std::shared_ptr<inner::description>> _actors;
  std::unordered_map<std::string, id_t> _id_by_name;
  std::unordered_map<id_t, std::shared_ptr<mailbox>> _mboxes;

  static std::atomic_size_t _ctx_id;
  bool _stopping_begin = false;

  actor_address _root;
  actor_address _usr_root;
  actor_address _sys_root;
};

} // namespace yaaf