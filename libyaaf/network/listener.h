#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/network/async_io.h>
#include <libyaaf/network/listener_client.h>
#include <libyaaf/utils/initialized_resource.h>

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace yaaf {
namespace network {
class listener;

class abstract_listener_consumer : public utils::initialized_resource {
public:
  EXPORT virtual ~abstract_listener_consumer();

  virtual void on_network_error(listener_client_ptr i, const network::message_ptr &d,
                                const boost::system::error_code &err) = 0;
  virtual void on_new_message(listener_client_ptr i, network::message_ptr &&d,
                              bool &cancel) = 0;
  virtual bool on_new_connection(listener_client_ptr i) = 0;
  virtual void on_disconnect(const listener_client_ptr &i) = 0;

  EXPORT void set_listener(const std::shared_ptr<listener> &lstnr);
  EXPORT bool is_listener_exists() const { return _lstnr != nullptr; }
  EXPORT void send_to(id_t id, network::message_ptr &d);

private:
  std::shared_ptr<listener> _lstnr;
};

using abstract_listener_consumer_ptr = abstract_listener_consumer *;

class listener : public std::enable_shared_from_this<listener>,
                 public utils::initialized_resource {
public:
  struct params {
    unsigned short port;
  };
  listener() = delete;
  listener(const listener &) = delete;

  EXPORT listener(boost::asio::io_service *service, params p);
  EXPORT virtual ~listener();
  EXPORT void start();
  EXPORT void stop();

  EXPORT void send_to(listener_client_ptr i, network::message_ptr &d);
  EXPORT void send_to(id_t id, network::message_ptr &d);

  EXPORT boost::asio::io_service *service() const { return _service; }

  EXPORT void erase_client_description(const listener_client_ptr client);
  EXPORT void add_consumer(const abstract_listener_consumer_ptr &c);

  EXPORT void erase_consumer();

  friend listener_client;

protected:
  void sendOk(listener_client_ptr i, uint64_t messageid);

  void on_network_error(listener_client_ptr i, const network::message_ptr &d,
                        const boost::system::error_code &err);
  void on_new_message(listener_client_ptr i, network::message_ptr &&d, bool &cancel);

private:
  void start_async_accept(network::async_io_ptr aio);

  EXPORT static void OnAcceptHandler(std::shared_ptr<listener> self,
                                     network::async_io_ptr aio,
                                     const boost::system::error_code &err);

protected:
  boost::asio::io_service *_service = nullptr;
  std::shared_ptr<boost::asio::ip::tcp::acceptor> _acc = nullptr;
  std::atomic_int _next_id;

  abstract_listener_consumer_ptr _consumer;

  std::mutex _locker_connections;
  std::list<listener_client_ptr> _connections;

  params _params;
};
} // namespace network
} // namespace yaaf
