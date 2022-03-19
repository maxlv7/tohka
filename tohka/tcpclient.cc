//
// Created by li on 2022/3/8.
//

#include "tcpclient.h"

#include "iobuf.h"
#include "tcpevent.h"
#include "util/log.h"

using namespace tohka;

TcpClient::TcpClient(IoWatcher* io_watcher, NetAddress& peer)
    : io_watcher_(io_watcher),
      connector_(std::make_shared<Connector>(io_watcher_, peer)),
      on_message_(DefaultOnMessage),
      on_connection_(DefaultOnConnection),
      connect_(true) {
  // socket writeable
  connector_->SetOnConnect(
      std::bind(&TcpClient::onConnect, this, std::placeholders::_1));
}
TcpClient::~TcpClient() {
  log_trace("TcpClient::~TcpClient");
  if (connection_) {
    // TODO ShutDown
    //    connection_->ShutDown();
  }
}
void TcpClient::Connect() {
  log_info("TcpClient::connect try to Connect to %s",
           connector_->GetPeerAddress().GetIpAndPort().c_str());
  connect_ = true;
  connector_->Start();
}
void TcpClient::Disconnect() {
  connect_ = false;
  // TODO conn->shutdown
}
void TcpClient::Stop() {
  connect_ = false;
  connector_->Stop();
}
void TcpClient::onConnect(int sock_fd) {
  auto name = connector_->GetPeerAddress().GetIpAndPort();
  log_info("Connect to %s fd = %d", name.c_str(), sock_fd);
  NetAddress peer_address = connector_->GetPeerAddress();
  auto new_conn =
      std::make_shared<TcpEvent>(io_watcher_, name, sock_fd, peer_address);

  new_conn->SetOnConnection(on_connection_);
  new_conn->SetOnOnMessage(on_message_);
  new_conn->SetOnWriteDone(on_write_done_);
  new_conn->SetOnClose(
      std::bind(&TcpClient::removeConnection, this, std::placeholders::_1));

  // 把新连接托管给tcp client管理
  connection_ = new_conn;
  new_conn->OnEstablishing();
}

void TcpClient::removeConnection(const TcpEventPrt_t& conn) {
  log_trace("TcpClient::removeConnection");
  assert(connection_ == conn);
  connection_.reset();
  conn->OnDestroying();
}
