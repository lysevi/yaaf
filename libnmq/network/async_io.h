#pragma once

#include <libnmq/network/message.h>
#include <libnmq/network/socket_ptr.h>
#include <libnmq/utils/async/locker.h>
#include <libnmq/utils/exception.h>
#include <atomic>
#include <functional>
#include <memory>

namespace nmq {
namespace network {

class AsyncIO : public std::enable_shared_from_this<AsyncIO> {
public:
  /// if method set 'cancel' to true, then read loop stoping.
  /// if dont_free_memory, then free NetData_ptr is in client side.
  using onDataRecvHandler = std::function<void(const Message_ptr &d, bool &cancel)>;
  using onNetworkErrorHandler =
      std::function<void(const Message_ptr &d, const boost::system::error_code &err)>;

  EXPORT AsyncIO(boost::asio::io_service *service, const socket_ptr &sock);
  EXPORT ~AsyncIO() noexcept(false);
  EXPORT void send(const Message_ptr d);
  EXPORT void start(onDataRecvHandler onRecv, onNetworkErrorHandler onErr);
  EXPORT void full_stop(bool waitAllMessages = false); /// stop thread, clean queue

  int queue_size() const { return _messages_to_send; }

private:
  void readNextAsync();

private:
  std::atomic_int _messages_to_send;
  boost::asio::io_service *_service = nullptr;

  socket_ptr _sock;

  bool _is_stoped;
  std::atomic_bool _begin_stoping_flag;
  Message::message_size_t next_message_size;

  onDataRecvHandler _on_recv_hadler;
  onNetworkErrorHandler _on_error_handler;
};
using AsyncIOPtr = std::shared_ptr<AsyncIO>;

} // namespace network
} // namespace nmq
