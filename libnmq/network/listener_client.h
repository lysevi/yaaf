#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>
#include <libnmq/types.h>
#include <atomic>
#include <mutex>

namespace nmq {
namespace network {

class listener;
class listenerClient : public std::enable_shared_from_this<listenerClient>, public utils::waitable {
public:
  listenerClient(id_t id_, network::async_io_ptr async_io, std::shared_ptr<listener> s);
  ~listenerClient();
  EXPORT void start();
  EXPORT void close();
  EXPORT void on_network_error(const message_ptr &d, const boost::system::error_code &err);
  EXPORT void on_data_recv(message_ptr &&d, bool &cancel);
  EXPORT void send_data(const message_ptr &d);
  EXPORT id_t get_id() const { return id; }

private:
  id_t id;
  network::async_io_ptr _async_connection = nullptr;
  std::shared_ptr<listener> _listener = nullptr;
};
using listener_client_ptr = std::shared_ptr<listenerClient>;
} // namespace network
} // namespace nmq