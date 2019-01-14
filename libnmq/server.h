#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/abstract_server.h>
#include <libnmq/network/async_io.h>
#include <libnmq/queries.h>
#include <libnmq/users.h>
#include <libnmq/utils/utils.h>

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nmq {

class Server : public network::AbstractServer,
               public utils::non_copy {
public:
  EXPORT Server(boost::asio::io_service *service, network::AbstractServer::Params &p);
  EXPORT virtual ~Server();
  /*EXPORT void serverStart();
  EXPORT void stopServer();
  EXPORT bool is_started();*/
  EXPORT std::vector<User> users() const;

  EXPORT void onStartComplete() override;
  EXPORT network::ON_NEW_CONNECTION_RESULT
  onNewConnection(ClientConnection_Ptr i) override;

  EXPORT void onNetworkError(ClientConnection_Ptr i, const NetworkMessage_ptr &d,
                             const boost::system::error_code &err) override;
  EXPORT void onNewMessage(ClientConnection_Ptr i, const NetworkMessage_ptr &d,
                           bool &cancel) override;
  EXPORT void onDisconnect(const AbstractServer::ClientConnection_Ptr &i) override;

protected:
  void sendOk(ClientConnection_Ptr i, uint64_t messageId);
protected:
  std::mutex _locker;
  uint64_t _nextMessageId = 0;
  UserBase_Ptr _users;
};
} // namespace nmq
