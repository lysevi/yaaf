#include <libnmq/queries.h>
#include <libnmq/server.h>
#include <libnmq/users.h>

#include <functional>
#include <string>

using namespace nmq;

Server::Server(boost::asio::io_service *service, Listener::Params &p)
    : Listener(service, p) {
  _users = UserBase::create();
  _users->append({"server", ServerID});
}

Server::~Server() {}

void Server::onNetworkError(ClientConnection_Ptr i, const network::Message_ptr &d,
                            const boost::system::error_code &err) {
  bool operation_aborted = err == boost::asio::error::operation_aborted;
  bool eof = err == boost::asio::error::eof;
  if (!operation_aborted && !eof) {
    std::string errm = err.message();
    logger_fatal("server: ", errm);
  }
}

void Server::onNewMessage(ClientConnection_Ptr i, const network::Message_ptr &d,
                          bool &cancel) {
  auto hdr = d->cast_to_header();

  switch (hdr->kind) {
  case MessageKinds::LOGIN: {
    logger_info("server: #", i->get_id(), " set login");
    queries::Login lg(d);

    network::Message_ptr nd = nullptr;
    if (onNewLogin(i, lg)) {
      queries::LoginConfirm lc(i->get_id());
      nd = lc.toNetworkMessage();
      sendTo(i, nd);
    } else {
      queries::LoginFailed lc(i->get_id());
      nd = lc.toNetworkMessage();
      sendTo(i, nd);
    }

    break;
  }
  default:
    THROW_EXCEPTION("unknow message kind: ", hdr->kind);
  }
}

bool Server::onNewLogin(const ClientConnection_Ptr i, const queries::Login &lg) {
  _users->setLogin(i->get_id(), lg.login);
  return true;
}

void Server::onStartComplete() {
  logger_info("server started.");
}

bool Server::onNewConnection(ClientConnection_Ptr i) {
  User cl;
  cl.id = i->get_id();
  cl.login = std::string("not set #") + std::to_string(cl.id);
  _users->append(cl);
  return true;
}

void Server::onDisconnect(const Listener::ClientConnection_Ptr &i) {
  _users->erase(i->get_id());
}

std::vector<User> Server::users() const {
  return _users->users();
}
