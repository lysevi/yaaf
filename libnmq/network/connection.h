#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>

namespace nmq {
namespace network {

class Connection : public std::enable_shared_from_this<Connection> {
public:
  struct Params {
    Params(std::string login_, std::string host_, unsigned short port_,
           bool auto_reconnection_ = true)
        : login(login_), host(host_), port(port_), auto_reconnection(auto_reconnection_) {

    }
    std::string login;
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
  EXPORT void reconnectOnError(const MessagePtr &d,
                               const boost::system::error_code &err);
  EXPORT void onDataReceive(const MessagePtr &d, bool &cancel);
  EXPORT void sendAsync(const MessagePtr &d);

  virtual void onConnect() = 0;
  virtual void onNewMessage(const MessagePtr &d, bool &cancel) = 0;
  virtual void onNetworkError(const MessagePtr &d,
                              const boost::system::error_code &err) = 0;

  EXPORT bool isConnected() const { return _isConnected; }
  EXPORT bool isStoped() const { return _isStoped; }
  
protected:
  std::shared_ptr<AsyncIO> _async_io = nullptr;
  boost::asio::io_service *_service = nullptr;
    bool _isConnected = false;
  bool _isStoped = false;
  Params _params;
};

} // namespace network
} // namespace nmq
