#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>
#include <libnmq/types.h>

#include <unordered_map>

namespace nmq {
namespace network {

class Connection;
class IConnectionConsumer {
public:
  EXPORT virtual ~IConnectionConsumer();
  virtual void onConnect() = 0;
  virtual void onNewMessage(MessagePtr &&d, bool &cancel) = 0;
  virtual void onNetworkError(const MessagePtr &d,
                              const boost::system::error_code &err) = 0;

  EXPORT bool isConnected() const;
  EXPORT bool isStoped() const;

  EXPORT void addConnection(std::shared_ptr<Connection> c);
  EXPORT bool isConnectionExists() const { return _connection != nullptr; }

private:
  std::shared_ptr<Connection> _connection;
};
using IConnectionConsumerPtr = IConnectionConsumer *;

class Connection : public std::enable_shared_from_this<Connection>,
                   public utils::Waitable {
public:
  struct Params {
    Params(std::string host_, unsigned short port_, bool auto_reconnection_ = true)
        : host(host_), port(port_), auto_reconnection(auto_reconnection_) {}
    std::string host;
    unsigned short port;
    bool auto_reconnection = true;
  };
  Connection() = delete;
  Params getParams() const { return _params; }

  EXPORT Connection(boost::asio::io_service *service, const Params &_parms);
  EXPORT virtual ~Connection();
  EXPORT void disconnect();
  EXPORT void startAsyncConnection();
  EXPORT void reconnectOnError(const MessagePtr &d, const boost::system::error_code &err);
  EXPORT void onDataReceive(MessagePtr &&d, bool &cancel);
  EXPORT void sendAsync(const MessagePtr &d);

  EXPORT void addConsumer(const IConnectionConsumerPtr &c);
  EXPORT void eraseConsumer();

protected:
  std::shared_ptr<AsyncIO> _async_io = nullptr;
  boost::asio::io_service *_service = nullptr;
  Params _params;

  IConnectionConsumerPtr _consumers;
};

} // namespace network
} // namespace nmq
