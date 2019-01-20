#include <libnmq/network/async_io.h>
#include <libnmq/utils/exception.h>
#include <libnmq/utils/utils.h>

using namespace boost::asio;
using namespace nmq;
using namespace nmq::network;

AsyncIO::AsyncIO(boost::asio::io_service *service) : _sock(*service) {
  _messages_to_send = 0;
  _is_stoped = true;
  ENSURE(service != nullptr);
  _service = service;
}

AsyncIO::~AsyncIO() noexcept(false) {
  fullStop();
}

void AsyncIO::start(data_handler_t onRecv, error_handler_t onErr) {
  if (!_is_stoped) {
    return;
  }
  _on_recv_hadler = onRecv;
  _on_error_handler = onErr;
  _is_stoped = false;
  _begin_stoping_flag = false;
  readNextAsync();
}

void AsyncIO::fullStop(bool waitAllMessages) {
  _begin_stoping_flag = true;
  try {

    if (_sock.is_open()) {
      if (waitAllMessages && _messages_to_send.load() != 0) {
        auto self = this->shared_from_this();
        _service->post([self]() { self->fullStop(); });
      } else {

        boost::system::error_code ec;
        _sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec) {
          auto message = ec.message();
          logger_fatal("AsyncIO::full_stop: _sock.shutdown() => code=", ec.value(),
                       " msg:", message);
        } else {
          _sock.close(ec);
          if (ec) {
            auto message = ec.message();
            logger_fatal("AsyncIO::full_stop: _sock.close(ec)  => code=", ec.value(),
                         " msg:", message);
          }
        }
        _service = nullptr;
        _is_stoped = true;
      }
    }

  } catch (...) {
  }
}

void AsyncIO::send(const MessagePtr d) {
  if (_begin_stoping_flag) {
    return;
  }
  auto self = shared_from_this();

  auto ds = d->asBuffer();

  _messages_to_send.fetch_add(1);
  auto buf = buffer(ds.data, ds.size);

  auto on_write = [self, d](auto err, auto /*read_bytes*/) {
    if (err) {
      self->_on_error_handler(d, err);
    }
    self->_messages_to_send.fetch_sub(1);
  };

  async_write(_sock, buf, on_write);
}

void AsyncIO::readNextAsync() {
  using utils::strings::args_to_string;

  auto self = shared_from_this();

  auto on_read_message = [self](auto err, auto read_bytes, auto data_left, MessagePtr d) {
    UNUSED(read_bytes);
    UNUSED(data_left);
    if (err) {
      self->_on_error_handler(d, err);
    } else {
      ENSURE_MSG(read_bytes == data_left,
                 args_to_string("exception on readNextAsync::async on_read_message ",
                                " - wrong size: expected ", data_left, " readed ",
                                read_bytes))
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

  auto on_read_size = [this, self, on_read_message](auto err, auto read_bytes) {
    UNUSED(read_bytes);
    if (err) {
      self->_on_error_handler(nullptr, err);
    } else {
      ENSURE_MSG(read_bytes == Message::SIZE_OF_SIZE,
                 args_to_string("exception on async readNextAsync::on_read_size. ",
                                " - wrong size: expected ", Message::SIZE_OF_SIZE,
                                " readed ", read_bytes));

      auto data_left = self->next_message_size - Message::SIZE_OF_SIZE;
      MessagePtr d = std::make_shared<Message>(data_left);

      auto buf_ptr = (uint8_t *)(d->data + Message::SIZE_OF_SIZE);
      auto buf = buffer(buf_ptr, data_left);
      auto callback = [self, on_read_message, data_left, d](auto err, auto read_bytes) {
        on_read_message(err, read_bytes, data_left, d);
      };
      async_read(self->_sock, buf, callback);
    };
  };
  auto buf = buffer(static_cast<void *>(&self->next_message_size), Message::SIZE_OF_SIZE);
  async_read(_sock, buf, on_read_size);
}
