#pragma once

namespace nmq {

template <typename Arg, typename Result> struct BaseIOChanel {
  struct Sender {
    Sender(BaseIOChanel<Arg, Result> &bc, nmq::Id id_) : chanel(bc) { id = id_; }
    BaseIOChanel<Arg, Result> &chanel;
    nmq::Id id;
  };

  struct ErrorCode {
    const boost::system::error_code &ec;
  };

  struct IOListener : public BaseIOChanel {

    virtual void onStartComplete() = 0;
    virtual void onError(const Sender &i, const ErrorCode &err) = 0;
    virtual void onMessage(const Sender &i, const Arg &d, bool &cancel) = 0;
    /**
    result - true for accept, false for failed.
    */
    virtual bool onClient(const Sender &i) = 0;
    virtual void onClientDisconnect(const Sender &i) = 0;
    virtual void sendAsync(nmq::Id client, const Result &message) = 0;
  };

  struct IOConnection : public BaseIOChanel {
    virtual void onConnected() = 0;
    virtual void onError(const ErrorCode &err) = 0;
    virtual void onMessage(const Result &d, bool &cancel) = 0;
    virtual void sendAsync(const Arg &message) = 0;
  };

  BaseIOChanel() { _next_message_id = 0; }

  virtual void start() = 0;
  virtual void stop() = 0;

  uint64_t getNextMessageId() { return _next_message_id.fetch_add(1); }

  std::atomic_uint64_t _next_message_id;
};
} // namespace nmq