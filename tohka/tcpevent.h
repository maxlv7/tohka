//
// Created by li on 2022/3/1.
//

#ifndef TOHKA_TOHKA_TCPEVENT_H
#define TOHKA_TOHKA_TCPEVENT_H


#include "iobuf.h"
#include "ioevent.h"
#include "log.h"
#include "netaddress.h"
#include "socket.h"
#include "tohka.h"

namespace tohka {

class IoWatcher;

// one tcp connection
class TcpEvent : noncopyable, public std::enable_shared_from_this<TcpEvent> {
 public:
  //  using TcpEventPrt = std::shared_ptr<TcpEvent>;
  //
  //  using OnMessageCallback =
  //      std::function<void(const TcpEventPrt& conn, IoBuf* buf)>;
  //  using OnConnectionCallback = std::function<void(const TcpEventPrt& conn)>;
  //  using OnCloseCallback = std::function<void(const TcpEventPrt& conn)>;

  TcpEvent(IoWatcher* io_watcher, std::string name, int fd, NetAddress& peer);
  ~TcpEvent();

  void StartReading() { event_->EnableReading(); }
  void StartWriting() { event_->EnableWriting(); }
  void StopReading() { event_->DisableReading(); }
  void StopWriting() { event_->DisableWriting(); }

  void SetOnConnection(const OnConnectionCallback& on_connection) {
    on_connection_ = on_connection;
  };
  void SetOnOnMessage(const OnMessageCallback& on_message) {
    on_message_ = on_message;
  };
  void SetOnClose(const OnCloseCallback& on_close) { on_close_ = on_close; }
  //  Be called when this connection establishing(libevent==on accept)
  void OnEstablishing();
  // Be called when this connection destroying (libevent == on close)
  void OnDestroying();

  void Send(std::string msg);
  void Send(const char* data, size_t len);

  bool Connected() { return state_ == kConnected; }
  std::string GetPeerIp() { return peer_.GetIp(); };
  uint16_t GetPeerPort() { return peer_.GetPort(); };
  std::string GetPeerIpAndPort() { return peer_.GetIpAndPort(); };

  std::string GetName() const { return name_; }

 private:
  void HandleRead();
  void HandleWrite();
  void HandleClose();
  void HandleError();

  enum STATE { kConnecting, kConnected, kDisconnecting, kDisconnected };
  IoWatcher* io_watcher_;
  std::unique_ptr<IoEvent> event_;
  std::unique_ptr<Socket> socket_;
  NetAddress peer_;
  std::string name_;
  STATE state_;
  IoBuf in_buf_;
  IoBuf out_buf_;

  void SetState(STATE state) { state_ = state; }
  OnMessageCallback on_message_;
  OnConnectionCallback on_connection_;
  OnCloseCallback on_close_;
};
}  // namespace tohka

#endif  // TOHKA_TOHKA_TCPEVENT_H
