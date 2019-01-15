#include <libnmq/network/async_io.h>
#include <libnmq/utils/exception.h>
#include <libnmq/utils/utils.h>

using namespace boost::asio;
using namespace nmq;
using namespace nmq::network;

AsyncIO::AsyncIO(onDataRecvHandler onRecv, onNetworkErrorHandler onErr) {
  _messages_to_send = 0;
  _is_stoped = true;
  _on_recv_hadler = onRecv;
  _on_error_handler = onErr;
}

AsyncIO::~AsyncIO() noexcept(false) {
  full_stop();
}

void AsyncIO::start(boost::asio::io_service *service, const socket_ptr &sock) {
  if (!_is_stoped) {
    return;
  }
  ENSURE(service != nullptr);
  _service = service;
  _sock = sock;
  _is_stoped = false;
  _begin_stoping_flag = false;
  readNextAsync();
}

void AsyncIO::full_stop(bool waitAllMessages) {
  _begin_stoping_flag = true;
  try {

    if (auto spt = _sock.lock()) {
      if (spt->is_open()) {
        if (waitAllMessages && _messages_to_send.load() != 0) {
          auto self = this->shared_from_this();
          _service->post([self]() { self->full_stop(); });
        } else {

          boost::system::error_code ec;
          spt->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
          if (ec) {
            auto message = ec.message();
            logger_fatal("AsyncIO::full_stop: ", message);
          }
          spt->close(ec);
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

void AsyncIO::send(const Message_ptr d) {
  if (_begin_stoping_flag) {
    return;
  }
  auto self = shared_from_this();

  auto ds = d->as_buffer();
  auto send_buffer = std::get<1>(ds);
  auto send_buffer_size = std::get<0>(ds);

  if (auto spt = _sock.lock()) {
    _messages_to_send.fetch_add(1);
    auto buf = buffer(send_buffer, send_buffer_size);
    async_write(*spt.get(), buf, [self, d](auto err, auto /*read_bytes*/) {
      if (err) {
        self->_on_error_handler(d, err);
      } else {
        self->_messages_to_send.fetch_sub(1);
        ENSURE(self->_messages_to_send >= 0);
      }
    });
  }
}

void AsyncIO::readNextAsync() {
  if (auto spt = _sock.lock()) {
    auto self = shared_from_this();

    auto on_read_size = [this, self, spt](auto err, auto read_bytes) {

      if (err) {
        self->_on_error_handler(nullptr, err);
      } else {
        if (read_bytes != Message::SIZE_OF_SIZE) {
          THROW_EXCEPTION("exception on async readNextAsync::on_read_size. ",
                          " - wrong size: expected ", Message::SIZE_OF_SIZE, " readed ",
                          read_bytes);
        }

        auto data_left = self->next_message_size - Message::SIZE_OF_SIZE;
        Message_ptr d = std::make_shared<Message>(data_left);

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

        auto buf_ptr = (uint8_t *)(d->data + Message::SIZE_OF_SIZE);
        auto buf = buffer(buf_ptr, data_left);
        async_read(*spt.get(), buf, on_read_message);
      }
    };

    async_read(*spt.get(),
               buffer((void *)&(self->next_message_size), Message::SIZE_OF_SIZE),
               on_read_size);
  }
}
