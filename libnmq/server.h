#pragma once

#include <libnmq/exports.h>
#include <libnmq/network/listener.h>
#include <libnmq/queries.h>
#include <libnmq/users.h>
#include <libnmq/utils/utils.h>

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nmq {

class Server : public network::Listener, public utils::non_copy {
public:
  EXPORT Server(boost::asio::io_service *service, network::Listener::Params &p);
  EXPORT virtual ~Server();
  EXPORT std::vector<User> users() const;

  EXPORT void onStartComplete() override;

  EXPORT bool onNewConnection(network::ListenerClient_Ptr i) override;
  EXPORT virtual bool onNewLogin(const network::ListenerClient_Ptr i,
                                 const queries::Login &lg);

  EXPORT void onNetworkError(network::ListenerClient_Ptr i,
                             const network::message_ptr &d,
                             const boost::system::error_code &err) override;
  EXPORT void onNewMessage(network::ListenerClient_Ptr i, const network::message_ptr &d,
                           bool &cancel) override;
  EXPORT void onDisconnect(const network::ListenerClient_Ptr &i) override;

protected:
  std::mutex _locker;
  UserBase_Ptr _users;
};
} // namespace nmq
