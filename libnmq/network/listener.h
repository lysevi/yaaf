#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>
#include <libnmq/network/listener_client.h>
#include <libnmq/users.h>
#include <atomic>
#include <mutex>

namespace nmq {
namespace network {

class Listener : public std::enable_shared_from_this<Listener> {
public:
  struct Params {
    unsigned short port;
  };

  EXPORT Listener(boost::asio::io_service *service, Params p);
  EXPORT virtual ~Listener();
  EXPORT void start();
  EXPORT void stop();

  EXPORT bool is_started() const { return _is_started; }
  EXPORT bool is_stoped() const { return _is_stoped; }

  EXPORT void sendTo(ListenerClient_Ptr i, network::message_ptr &d);
  EXPORT void sendTo(Id id, network::message_ptr &d);

  EXPORT boost::asio::io_service *service() const { return _service; }

  EXPORT void erase_client_description(const ListenerClient_Ptr client);

  virtual void onStartComplete() = 0;
  virtual void onNetworkError(ListenerClient_Ptr i, const network::message_ptr &d,
                              const boost::system::error_code &err) = 0;
  virtual void onNewMessage(ListenerClient_Ptr i, const network::message_ptr &d,
                            bool &cancel) = 0;
  /**
  result - true for accept, false for failed.
  */
  virtual bool onNewConnection(ListenerClient_Ptr i) = 0;
  virtual void onDisconnect(const ListenerClient_Ptr &i) = 0;

protected:
  void sendOk(ListenerClient_Ptr i, uint64_t messageId);

private:
  void start_accept(network::AsyncIOPtr aio);

  static void handle_accept(std::shared_ptr<Listener> self, network::AsyncIOPtr aio,
                            const boost::system::error_code &err);

protected:
  boost::asio::io_service *_service = nullptr;
  std::shared_ptr<boost::asio::ip::tcp::acceptor> _acc = nullptr;
  bool _is_started = false;
  std::atomic_int _next_id;

  std::mutex _locker_connections;
  std::list<ListenerClient_Ptr> _connections;
  Params _params;
  bool _begin_stoping=false;
  bool _is_stoped = false;
};
} // namespace network
} // namespace nmq
