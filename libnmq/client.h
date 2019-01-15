#pragma once

#include <libnmq/exports.h>
#include <libnmq/kinds.h>
#include <libnmq/network/connection.h>
#include <libnmq/queries.h>
#include <libnmq/utils/async/locker.h>
#include <libnmq/utils/utils.h>
#include <boost/asio.hpp>
#include <cstring>
#include <shared_mutex>

namespace nmq {

struct AsyncOperationResult {
  std::shared_ptr<utils::async::locker> locker;
  static AsyncOperationResult makeNew() {
    AsyncOperationResult result;
    result.locker = std::make_shared<utils::async::locker>();
    return result;
  }
};

class Client : public network::Connection, public utils::non_copy {
public:
  Client() = delete;
  EXPORT Client(boost::asio::io_service *service, const Connection::Params &_params);
  EXPORT ~Client();

  EXPORT void connect();
  EXPORT void connectAsync();
  EXPORT void disconnect();
  EXPORT bool is_connected();

  EXPORT void waitAll() const;
  uint64_t getId() const { return _id; }

  EXPORT void onConnect() override;

  EXPORT void onNetworkError(const network::Message_ptr &d,
                             const boost::system::error_code &err) override;

  EXPORT virtual void onMessage(const std::string & /*queueName*/,
                                const std::vector<uint8_t> & /*d*/){};

private:
  EXPORT void onNewMessage(const network::Message_ptr &d, bool &cancel) override;
  void send(const network::Message_ptr &nd);

  uint64_t getNextId() { return _nextMessageId++; }

  /*AsyncOperationResult makeNewQResult(uint64_t msgId) {
    auto qr = AsyncOperationResult::makeNew();
    _queries[msgId] = qr;
    qr.locker->lock();
    return qr;
  }*/

protected:
  mutable std::shared_mutex _locker;
  uint64_t _nextMessageId = 0;
  bool _loginConfirmed = false;
  uint64_t _id = 0;
  std::map<uint64_t, AsyncOperationResult> _queries;
};
} // namespace nmq
