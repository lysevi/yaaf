#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/network/async_io.h>
#include <libyaaf/network/initialized_resource.h>
#include <atomic>
#include <mutex>

namespace yaaf {
namespace network {

class listener;
class listener_client : public std::enable_shared_from_this<listener_client>,
                       public initialized_resource {
public:
  listener_client(uint64_t id_, network::async_io_ptr async_io, std::shared_ptr<listener> s);
  ~listener_client();
  EXPORT void start();
  EXPORT void close();
  EXPORT void on_network_error(const message_ptr &d,
                               const boost::system::error_code &err);
  EXPORT void on_data_recv(message_ptr &&d, bool &cancel);
  EXPORT void send_data(const message_ptr &d);
  EXPORT uint64_t get_id() const { return id; }

private:
  uint64_t id;
  network::async_io_ptr _async_connection = nullptr;
  std::shared_ptr<listener> _listener = nullptr;
};
using listener_client_ptr = std::shared_ptr<listener_client>;
} // namespace network
} // namespace yaaf
