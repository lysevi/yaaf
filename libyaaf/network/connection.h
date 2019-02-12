#pragma once

#include <libyaaf/exports.h>
#include <libyaaf/network/async_io.h>
#include <libyaaf/types.h>
#include <libyaaf/utils/initialized_resource.h>

#include <unordered_map>

namespace yaaf {
namespace network {

class connection;
class abstract_connection_consumer {
public:
  EXPORT virtual ~abstract_connection_consumer();
  virtual void on_connect() = 0;
  virtual void on_new_message(message_ptr &&d, bool &cancel) = 0;
  virtual void on_network_error(const message_ptr &d,
                                const boost::system::error_code &err) = 0;

  EXPORT bool is_connected() const;
  EXPORT bool is_stoped() const;

  EXPORT void add_connection(std::shared_ptr<connection> c);
  EXPORT bool is_connection_exists() const { return _connection != nullptr; }

private:
  std::shared_ptr<connection> _connection;
};
using abstract_connection_consumer_ptr = abstract_connection_consumer *;

class connection : public std::enable_shared_from_this<connection>,
                   public utils::initialized_resource {
public:
  struct params_t {
    params_t(std::string host_, unsigned short port_, bool auto_reconnection_ = true)
        : host(host_), port(port_), auto_reconnection(auto_reconnection_) {}
    std::string host;
    unsigned short port;
    bool auto_reconnection = true;
  };
  connection() = delete;
  params_t get_params() const { return _params; }

  EXPORT connection(boost::asio::io_service *service, const params_t &_parms);
  EXPORT virtual ~connection();
  EXPORT void disconnect();
  EXPORT void start_async_connection();
  EXPORT void reconnecton_error(const message_ptr &d,
                                const boost::system::error_code &err);
  EXPORT void on_data_receive(message_ptr &&d, bool &cancel);
  EXPORT void send_async(const message_ptr &d);

  EXPORT void add_consumer(const abstract_connection_consumer_ptr &c);
  EXPORT void erase_consumer();

protected:
  std::shared_ptr<async_io> _async_io = nullptr;
  boost::asio::io_service *_service = nullptr;
  params_t _params;

  abstract_connection_consumer_ptr _consumers;
};

} // namespace network
} // namespace yaaf
