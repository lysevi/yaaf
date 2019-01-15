#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/async_io.h>
#include <libnmq/users.h>
#include <atomic>
#include <mutex>

namespace nmq {
namespace network {

enum class ON_NEW_CONNECTION_RESULT { ACCEPT, DISCONNECT };

class Listener : public std::enable_shared_from_this<Listener> {
public:
  struct Params {
    unsigned short port;
  };

  class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
  public:
    ClientConnection(Id id_, network::AsyncIOPtr async_io, std::shared_ptr<Listener> s);
    ~ClientConnection();
    void start();
    void close();
    void onNetworkError(const Message_ptr &d, const boost::system::error_code &err);
    void onDataRecv(const Message_ptr &d, bool &cancel);
    EXPORT void sendData(const Message_ptr &d);
    EXPORT Id get_id() const { return id; }

  private:
    Id id;
    network::AsyncIOPtr _async_connection = nullptr;
    std::shared_ptr<Listener> _listener = nullptr;
  };
  using ClientConnection_Ptr = std::shared_ptr<ClientConnection>;

  EXPORT Listener(boost::asio::io_service *service, Params p);
  EXPORT virtual ~Listener();
  EXPORT void start();
  EXPORT void stop();

  EXPORT bool is_started() const { return _is_started; }
  EXPORT bool is_stoped() const { return _is_stoped; }

  EXPORT void sendTo(ClientConnection_Ptr i, network::Message_ptr &d);
  EXPORT void sendTo(Id id, network::Message_ptr &d);

  EXPORT boost::asio::io_service *service() const { return _service; }

  virtual void onStartComplete() = 0;
  virtual void onNetworkError(ClientConnection_Ptr i, const network::Message_ptr &d,
                              const boost::system::error_code &err) = 0;
  virtual void onNewMessage(ClientConnection_Ptr i, const network::Message_ptr &d,
                            bool &cancel) = 0;
  virtual ON_NEW_CONNECTION_RESULT onNewConnection(ClientConnection_Ptr i) = 0;
  virtual void onDisconnect(const ClientConnection_Ptr &i) = 0;

protected:
  void sendOk(ClientConnection_Ptr i, uint64_t messageId);

private:
  void start_accept(socket_ptr sock);

  static void handle_accept(std::shared_ptr<Listener> self, socket_ptr sock,
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
