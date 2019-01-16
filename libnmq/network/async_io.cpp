#include <boost/asio.hpp>
#include <libnmq/network/async_io.h>
#include <libnmq/utils/exception.h>
#include <libnmq/utils/utils.h>

using namespace boost::asio;
using namespace nmq;
using namespace nmq::network;

async_io::async_io(boost::asio::io_service *service) : _sock(*service) {
  _messages_to_send = 0;
  _is_stoped = true;
  ENSURE(service != nullptr);
  _service = service;
}

async_io::~async_io() noexcept(false) {
  full_stop();
}

void async_io::start(data_handler_t onRecv, error_handler_t onErr) {
  if (!_is_stoped) {
    return;
  }
  _on_recv_hadler = onRecv;
  _on_error_handler = onErr;
  _is_stoped = false;
  _begin_stoping_flag = false;
  ENSURE(auto spt = _sock.lock());
  readNextAsync();
}

void async_io::full_stop(bool waitAllMessages) {
  _begin_stoping_flag = true;
  try {
    ENSURE(auto spt = _sock.lock());
    // if (auto spt = _sock.lock())
    {
      if (_sock.is_open()) {
        if (waitAllMessages && _messages_to_send.load() != 0) {
          auto self = this->shared_from_this();
          _service->post([self]() { self->full_stop(); });
        } else {

          boost::system::error_code ec;
          _sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
          if (ec) {
            auto message = ec.message();
            logger_fatal("AsyncIO::full_stop: ", message);
          }
          _sock.close(ec);
          if (ec) {
            auto message = ec.message();
            logger_fatal("AsyncIO::full_stop: ", message);
          }
          _service = nullptr;
        }
      }
    }

  } catch (...) {
  }
}

void async_io::send(const message_ptr d) {
  if (_begin_stoping_flag) {
    return;
  }
  auto self = shared_from_this();

  auto ds = d->as_buffer();
  auto send_buffer = std::get<1>(ds);
  auto send_buffer_size = std::get<0>(ds);
  ENSURE(auto spt = _sock.lock());
  // if (auto spt = _sock.lock())
  {
    _messages_to_send.fetch_add(1);
    auto buf = buffer(send_buffer, send_buffer_size);
    async_write(_sock, buf, [self, d](auto err, auto /*read_bytes*/) {
      if (err) {
        self->_on_error_handler(d, err);
      } else {
        self->_messages_to_send.fetch_sub(1);
        ENSURE(self->_messages_to_send >= 0);
      }
    });
  }
}

void async_io::readNextAsync() {
  // if (auto spt = _sock.lock())
  {
    auto self = shared_from_this();

    auto on_read_size = [this, self](auto err, auto read_bytes) {
      if (err) {
        self->_on_error_handler(nullptr, err);
      } else {
        if (read_bytes != message::SIZE_OF_SIZE) {
          THROW_EXCEPTION("exception on async readNextAsync::on_read_size. ",
                          " - wrong size: expected ", message::SIZE_OF_SIZE, " readed ",
                          read_bytes);
        }

        auto data_left = self->next_message_size - message::SIZE_OF_SIZE;
        message_ptr d = std::make_shared<message>(data_left);

        auto on_read_message = [self, d, data_left](auto err, auto read_bytes) {
          if (err) {
            self->_on_error_handler(d, err);
          } else {
            if (read_bytes != data_left) {
              THROW_EXCEPTION("exception on async readNextAsync. ",
                              " - wrong size: expected ", data_left, " readed ",
                              read_bytes);
            }
            bool cancel_flag = false;
            try {
              self->_on_recv_hadler(d, cancel_flag);
            } catch (std::exception &ex) {
              THROW_EXCEPTION("exception on async readNextAsync::on_read_message. - ",
                              ex.what());
            }
            if (!cancel_flag) {
              self->readNextAsync();
            }
          }
        };

        auto buf_ptr = (uint8_t *)(d->data + message::SIZE_OF_SIZE);
        auto buf = buffer(buf_ptr, data_left);
        async_read(self->_sock, buf, on_read_message);
      }
    };

    async_read(_sock, buffer((void *)&(self->next_message_size), message::SIZE_OF_SIZE),
               on_read_size);
  }
}
