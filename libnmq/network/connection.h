#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>
#include <boost/asio.hpp>

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
  EXPORT void async_connect();
  EXPORT void reconnectOnError(const Message_ptr &d,
                               const boost::system::error_code &err);
  EXPORT void dataRecv(const Message_ptr &d, bool &cancel);

  virtual void onConnect() = 0;
  virtual void onNewMessage(const Message_ptr &d, bool &cancel) = 0;
  virtual void onNetworkError(const Message_ptr &d,
                              const boost::system::error_code &err) = 0;

  EXPORT bool is_connected() const { return isConnected; }
  EXPORT bool is_stoped() const { return isStoped; }

protected:
  std::shared_ptr<AsyncIO> _async_connection = nullptr;
  boost::asio::io_service *_service = nullptr;
  //socket_ptr _socket = nullptr;
  bool isConnected = false;
  bool isStoped = false;
  Params _params;
};

} // namespace network
} // namespace nmq
