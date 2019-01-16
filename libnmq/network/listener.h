#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>
#include <libnmq/network/listener_client.h>

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
  
  EXPORT bool isStopingBegin() const { return _begin_stoping; }
  EXPORT bool isStarted() const { return _is_started; }
  EXPORT bool isStoped() const { return _is_stoped; }

  EXPORT void sendTo(ListenerClientPtr i, network::MessagePtr &d);
  EXPORT void sendTo(Id id, network::MessagePtr &d);

  EXPORT boost::asio::io_service *service() const { return _service; }

  EXPORT void eraseClientDescription(const ListenerClientPtr client);

  virtual void onStartComplete() = 0;
  virtual void onNetworkError(ListenerClientPtr i, const network::MessagePtr &d,
                              const boost::system::error_code &err) = 0;
  virtual void onNewMessage(ListenerClientPtr i, const network::MessagePtr &d,
                            bool &cancel) = 0;
  /**
  result - true for accept, false for failed.
  */
  virtual bool onNewConnection(ListenerClientPtr i) = 0;
  virtual void onDisconnect(const ListenerClientPtr &i) = 0;

protected:
  void sendOk(ListenerClientPtr i, uint64_t messageId);

private:
  void startAsyncAccept(network::AsyncIOPtr aio);

  static void OnAcceptHandler(std::shared_ptr<Listener> self, network::AsyncIOPtr aio,
                              const boost::system::error_code &err);

protected:
  boost::asio::io_service *_service = nullptr;
  std::shared_ptr<boost::asio::ip::tcp::acceptor> _acc = nullptr;
  bool _is_started = false;
  std::atomic_int _next_id;

  std::mutex _locker_connections;
  std::list<ListenerClientPtr> _connections;
  Params _params;
  bool _begin_stoping = false;
  bool _is_stoped = false;
};
} // namespace network
} // namespace nmq
