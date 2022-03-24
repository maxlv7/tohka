//
// Created by li on 2022/3/2.
//

#include "tcpserver.h"

using namespace tohka;
TcpServer::TcpServer(IoWatcher* io_watcher, NetAddress& bind_address)
    : io_watcher_(io_watcher),
      acceptor_(std::make_unique<Acceptor>(io_watcher, bind_address)),
      on_connection_(DefaultOnConnection),
      on_message_(DefaultOnMessage),
      conn_id_(1) {
  acceptor_->SetOnAccept(std::bind(&TcpServer::OnAccept, this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
}
TcpServer::~TcpServer() {
  for (const auto& item : connection_map_) {
    item.second->ConnectDestroyed();
  }
}

// call OnConnectionCallback
void TcpServer::OnAccept(int conn_fd, NetAddress& peer_address) {
  auto name = peer_address.GetIpAndPort() + "#" + std::to_string(conn_id_);
  log_info("[TcpServer::OnAccept]->new connection from %s fd = %d",
           name.c_str(), conn_fd);
  ++conn_id_;
  auto new_conn =
      std::make_shared<TcpEvent>(io_watcher_, name, conn_fd, peer_address);

  //  connection_map_[name] = new_conn;
  connection_map_.emplace(name, new_conn);
  // call user callback
  new_conn->SetOnConnection(on_connection_);
  new_conn->SetOnOnMessage(on_message_);
  // TODO on write done
  new_conn->SetOnClose(
      std::bind(&TcpServer::OnClose, this, std::placeholders::_1));

  // call ConnectEstablished
  new_conn->ConnectEstablished();
}
void TcpServer::OnClose(const TcpEventPrt_t& conn) {
  auto name = conn->GetName();
  int fd = conn->GetFd();
  // remove from conn map
  auto status = connection_map_.erase(name);
  assert(status == 1);
  log_info("[TcpServer::OnClose]->remove connection from %s fd = %d",
           name.c_str(), fd);
  conn->ConnectDestroyed();
}
void TcpServer::Run() { acceptor_->Listen(); }
