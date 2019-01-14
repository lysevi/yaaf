#include <libnmq/queries.h>
#include <libnmq/server.h>
#include <libnmq/users.h>

#include <functional>
#include <string>

using namespace nmq;

Server::Server(boost::asio::io_service *service, AbstractServer::Params &p)
    : AbstractServer(service, p) {
  _users = UserBase::create();
  _users->append({"server", ServerID});
}

Server::~Server() {}

void Server::onNetworkError(ClientConnection_Ptr i, const NetworkMessage_ptr &d,
                            const boost::system::error_code &err) {
  bool operation_aborted = err == boost::asio::error::operation_aborted;
  bool eof = err == boost::asio::error::eof;
  if (!operation_aborted && !eof) {
    std::string errm = err.message();
    logger_fatal("server: ", errm);
  }
}

void Server::onNewMessage(ClientConnection_Ptr i, const NetworkMessage_ptr &d,
                          bool &cancel) {
  auto hdr = d->cast_to_header();

  switch (hdr->kind) {
  case (NetworkMessage::message_kind)MessageKinds::LOGIN: {
    logger_info("server: #", i->get_id(), " set login");
    queries::Login lg(d);
    _users->setLogin(i->get_id(), lg.login);

    queries::LoginConfirm lc(i->get_id());
    auto nd = lc.toNetworkMessage();
    sendTo(i, nd);
    break;
  }
  default:
    THROW_EXCEPTION("unknow message kind: ", hdr->kind);
  }
}

void Server::sendOk(ClientConnection_Ptr i, uint64_t messageId) {
  auto nd = queries::Ok(messageId).toNetworkMessage();
  this->sendTo(i, nd);
}

void Server::onStartComplete() {
  logger_info("server started.");
}

network::ON_NEW_CONNECTION_RESULT Server::onNewConnection(ClientConnection_Ptr i) {
  User cl;
  cl.id = i->get_id();
  cl.login = "not set";
  _users->append(cl);
  return network::ON_NEW_CONNECTION_RESULT::ACCEPT;
}

void Server::onDisconnect(const AbstractServer::ClientConnection_Ptr &i) {
  _users->erase(i->get_id());
}

std::vector<User> Server::users() const {
  return _users->users();
}
