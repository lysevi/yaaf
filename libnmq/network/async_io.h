#pragma once

#include <libnmq/network/message.h>
#include <libnmq/utils/async/locker.h>
#include <libnmq/utils/exception.h>

#include <boost/asio.hpp>

#include <atomic>
#include <functional>
#include <memory>

namespace nmq {
namespace network {

class async_io : public std::enable_shared_from_this<async_io> {
public:
  /// if method set 'cancel' to true, then read loop stoping.
  /// if dont_free_memory, then free NetData_ptr is in client side.
  using data_handler_t = std::function<void(const message_ptr &d, bool &cancel)>;
  using error_handler_t =
      std::function<void(const message_ptr &d, const boost::system::error_code &err)>;

  EXPORT async_io(boost::asio::io_service *service);
  EXPORT ~async_io() noexcept(false);
  EXPORT void send(const message_ptr d);
  EXPORT void start(data_handler_t onRecv, error_handler_t onErr);
  EXPORT void full_stop(bool waitAllMessages = false); /// stop thread, clean queue

  int queue_size() const { return _messages_to_send; }
  boost::asio::ip::tcp::socket &socket() { return _sock; }

private:
  void readNextAsync();

private:
  std::atomic_int _messages_to_send;
  boost::asio::io_service *_service = nullptr;

  boost::asio::ip::tcp::socket _sock;

  bool _is_stoped;
  std::atomic_bool _begin_stoping_flag;
  message::size_t next_message_size;

  data_handler_t _on_recv_hadler;
  error_handler_t _on_error_handler;
};
using AsyncIOPtr = std::shared_ptr<async_io>;

} // namespace network
} // namespace nmq
