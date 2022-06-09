//
// Created by li on 2022/3/8.
//

#include "tcpclient.h"

#include "iobuf.h"
#include "ioloop.h"
#include "tcpevent.h"
#include "util/log.h"
using namespace tohka;

TcpClient::TcpClient(IoLoop* loop, const NetAddress& peer, std::string name)
    : loop_(loop),
      connector_(std::make_unique<Connector>(loop_, peer)),
      retry_(true),
      connect_(true),
      conn_id_(1),
      on_connection_(DefaultOnConnection),
      on_message_(DefaultOnMessage),
      name_(std::move(name)) {
  // socket writeable
  connector_->SetOnConnect([this](int sock_fd) { OnConnect(sock_fd); });
}
TcpClient::~TcpClient() {
  log_debug("[TcpClient::~TcpClient]");
  // assert(connection_ == nullptr);
  // 对方还没断开连接或者自己没有主动断开连接
  // 这时需要清除掉io_events_map和对应的fd
  if (connection_) {
    // 直接暴力关掉
    //    assert(connection_.use_count() == 1);
    if (connection_.use_count() == 1) {
      connection_->ForceClose();
    }
    log_trace("TcpClient::~TcpClient()");
  } else {
    log_trace("TcpClient::~TcpClient()");
    connector_->Stop();
  }
}
void TcpClient::Connect() {
  log_info("[TcpClient::connect]->try to Connect to %s",
           connector_->GetPeerAddress().GetIpAndPort().c_str());
  connector_->Start();
  if (normal_callback_) {
    // Hint 这个时候有可能TcpClient被析构了
    IoLoop::GetLoop()->CallLater(5000, [this] { OnTimeOut(); });
  }
}
void TcpClient::Disconnect() {
  // 连接端已经正式连接成功,这里我们需要关闭这个连接 释放内存
  if (connection_) {
    log_info("TcpClient::Disconnect()");
    connection_->ShutDown();
  } else {
    log_warn("TcpClient::Disconnect no connection connected!");
  }
}
void TcpClient::Stop() { connector_->Stop(); }
void TcpClient::OnConnect(int sock_fd) {
  auto name = connector_->GetPeerAddress().GetIpAndPort() + "#" +
              std::to_string(conn_id_);
  ++conn_id_;
  log_info("[TcpClient::onConnect]->Connected to %s fd = %d", name.c_str(),
           sock_fd);
  NetAddress peer_address = connector_->GetPeerAddress();
  auto new_conn =
      std::make_shared<TcpEvent>(loop_, name, sock_fd, peer_address);

  new_conn->SetOnConnection(on_connection_);
  new_conn->SetOnOnMessage(on_message_);
  new_conn->SetOnWriteDone(on_write_done_);
  // handle close
  // 在连接关闭时清除掉pollfd和对应的ioevent

  new_conn->SetOnClose(
      [this](const TcpEventPrt_t& conn) { RemoveConnection(conn); });
  // 把新连接托管给tcp client管理
  connection_ = new_conn;
  new_conn->ConnectEstablished();
}
void TcpClient::OnTimeOut() {
  // 如果没有连接成功的话，那么调用
  if (!connection_) {
    normal_callback_();
  }
}
void TcpClient::RemoveConnection(const TcpEventPrt_t& conn) {
  log_info("[TcpClient::removeConnection]->remove connection from %s fd = %d",
           connector_->GetPeerAddress().GetIpAndPort().c_str(), conn->GetFd());
  assert(connection_ == conn);
  connection_.reset();
  // remove from pollfd
  conn->ConnectDestroyed();
  //  if (retry_ && connect_) {
  //    log_info("[TcpClient::removeConnection]->try to reconnecting to %s",
  //             connector_->GetPeerAddress().GetIpAndPort().c_str());
  //    connector_->Restart();
  //  }
}
