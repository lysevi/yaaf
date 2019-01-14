#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>
#include <libnmq/users.h>
#include <mutex>

namespace nmq {
namespace network {

enum class ON_NEW_CONNECTION_RESULT { ACCEPT, DISCONNECT };

class AbstractServer : public std::enable_shared_from_this<AbstractServer> {
public:
  struct Params {
    unsigned short port;
  };

  class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
  public:
    ClientConnection(Id id_, socket_ptr sock_, std::shared_ptr<AbstractServer> s);
    ~ClientConnection();
    void start();
    void close();
    void onNetworkError(const NetworkMessage_ptr &d,
                        const boost::system::error_code &err);
    void onDataRecv(const NetworkMessage_ptr &d, bool &cancel);
    EXPORT void sendData(const NetworkMessage_ptr &d);
    EXPORT Id get_id() const { return id; }

  private:
    Id id;
    socket_ptr sock = nullptr;
    std::shared_ptr<AsyncIO> _async_connection = nullptr;
    std::shared_ptr<AbstractServer> _server = nullptr;
  };
  using ClientConnection_Ptr = std::shared_ptr<ClientConnection>;

  EXPORT AbstractServer(boost::asio::io_service *service, Params p);
  EXPORT virtual ~AbstractServer();
  EXPORT void serverStart();
  EXPORT void stopServer();
  EXPORT void start_accept(socket_ptr sock);
  EXPORT bool is_started() const { return _is_started; }
  EXPORT bool is_stoped() const { return _is_stoped; }
  EXPORT void sendTo(ClientConnection_Ptr i, NetworkMessage_ptr &d);
  EXPORT void sendTo(Id id, NetworkMessage_ptr &d);

  virtual void onStartComplete() = 0;

  virtual void onNetworkError(ClientConnection_Ptr i, const NetworkMessage_ptr &d,
                              const boost::system::error_code &err) = 0;
  virtual void onNewMessage(ClientConnection_Ptr i, const NetworkMessage_ptr &d,
                            bool &cancel) = 0;
  virtual ON_NEW_CONNECTION_RESULT onNewConnection(ClientConnection_Ptr i) = 0;
  virtual void onDisconnect(const ClientConnection_Ptr &i) = 0;

private:
  static void handle_accept(std::shared_ptr<AbstractServer> self, socket_ptr sock,
                            const boost::system::error_code &err);
  void erase_client_description(const ClientConnection_Ptr client);

protected:
  boost::asio::io_service *_service = nullptr;
  std::shared_ptr<boost::asio::ip::tcp::acceptor> _acc = nullptr;
  bool _is_started = false;
  std::atomic_int _next_id;
  std::mutex _locker_connections;
  std::list<ClientConnection_Ptr> _connections;
  Params _params;
  bool _is_stoped = false;
};
} // namespace network
} // namespace nmq
