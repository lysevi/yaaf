#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>
#include <libnmq/network/listener_client.h>

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace nmq {
namespace network {
class Listener;

class IListenerConsumer : public utils::Waitable {
public:
  EXPORT virtual ~IListenerConsumer();

  virtual void onNetworkError(ListenerClientPtr i, const network::MessagePtr &d,
                              const boost::system::error_code &err) = 0;
  virtual void onNewMessage(ListenerClientPtr i, network::MessagePtr &&d,
                            bool &cancel) = 0;
  virtual bool onNewConnection(ListenerClientPtr i) = 0;
  virtual void onDisconnect(const ListenerClientPtr &i) = 0;

  EXPORT void setListener(const std::shared_ptr<Listener> &lstnr);
  EXPORT bool isListenerExists() const { return _lstnr != nullptr; }
  EXPORT void sendTo(Id id, network::MessagePtr &d);

private:
  std::shared_ptr<Listener> _lstnr;
};

using IListenerConsumerPtr = IListenerConsumer *;

class Listener : public std::enable_shared_from_this<Listener>, public utils::Waitable {
public:
  struct Params {
    unsigned short port;
  };
  Listener() = delete;
  Listener(const Listener &) = delete;

  EXPORT Listener(boost::asio::io_service *service, Params p);
  EXPORT virtual ~Listener();
  EXPORT void start();
  EXPORT void stop();

  EXPORT void sendTo(ListenerClientPtr i, network::MessagePtr &d);
  EXPORT void sendTo(Id id, network::MessagePtr &d);

  EXPORT boost::asio::io_service *service() const { return _service; }

  EXPORT void eraseClientDescription(const ListenerClientPtr client);
  EXPORT void addConsumer(const IListenerConsumerPtr &c);

  EXPORT void eraseConsumer();

  friend ListenerClient;

protected:
  void sendOk(ListenerClientPtr i, uint64_t messageId);

  void onNetworkError(ListenerClientPtr i, const network::MessagePtr &d,
                      const boost::system::error_code &err);
  void onNewMessage(ListenerClientPtr i, network::MessagePtr &&d, bool &cancel);

private:
  void startAsyncAccept(network::AsyncIOPtr aio);

  EXPORT static void OnAcceptHandler(std::shared_ptr<Listener> self,
                                     network::AsyncIOPtr aio,
                                     const boost::system::error_code &err);

protected:
  boost::asio::io_service *_service = nullptr;
  std::shared_ptr<boost::asio::ip::tcp::acceptor> _acc = nullptr;
  std::atomic_int _next_id;

  IListenerConsumerPtr _consumer;

  std::mutex _locker_connections;
  std::list<ListenerClientPtr> _connections;

  Params _params;
};
} // namespace network
} // namespace nmq
