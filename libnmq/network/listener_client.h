#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>
#include <libnmq/types.h>
#include <atomic>
#include <mutex>

namespace nmq {
namespace network {

class Listener;
class ListenerClient : public std::enable_shared_from_this<ListenerClient> {
public:
  ListenerClient(Id id_, network::AsyncIOPtr async_io, std::shared_ptr<Listener> s);
  ~ListenerClient();
  EXPORT void start();
  EXPORT void close();
  EXPORT void onNetworkError(const MessagePtr &d, const boost::system::error_code &err);
  EXPORT void onDataRecv(const MessagePtr &d, bool &cancel);
  EXPORT void sendData(const MessagePtr &d);
  EXPORT Id get_id() const { return id; }

private:
  Id id;
  network::AsyncIOPtr _async_connection = nullptr;
  std::shared_ptr<Listener> _listener = nullptr;
};
using ListenerClientPtr = std::shared_ptr<ListenerClient>;
} // namespace network
} // namespace nmq