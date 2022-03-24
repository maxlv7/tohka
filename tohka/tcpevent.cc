//
// Created by li on 2022/3/1.
//

#include "tcpevent.h"
using namespace tohka;

void tohka::DefaultOnConnection(const TcpEventPrt_t& conn) {
  log_trace("connection status = %s",
            conn->Connected() == true ? "up" : "down");
}

void tohka::DefaultOnMessage(const TcpEventPrt_t& conn, IoBuf* buf) {
  buf->ReceiveAllAsString();
}

TcpEvent::TcpEvent(IoWatcher* io_watcher, std::string name, int fd,
                   NetAddress& peer)
    : io_watcher_(io_watcher),
      event_(std::make_unique<IoEvent>(io_watcher, fd)),
      socket_(std::make_unique<Socket>(fd)),
      peer_(peer),
      name_(std::move(name)),
      state_(kConnecting) {
  socket_->SetKeepAlive(true);

  event_->SetReadCallback([this] { HandleRead(); });
  event_->SetWriteCallback([this] { HandleWrite(); });
  event_->SetCloseCallback([this] { HandleClose(); });
  event_->SetErrorCallback([this] { HandleError(); });
}

void TcpEvent::HandleRead() {
  log_trace("TcpEvent::HandleRead fd = %d", socket_->GetFd());
  // TODO debug only(set read ext_buf size=1)
  ssize_t n;
  // TODO only support unix (support windows)
#if defined(OS_UNIX)
  char ext_buf[65535];
  struct iovec vec[2];
  //  size_t writeable_size = in_buf_.GetWriteableSize();
  vec[0].iov_base = in_buf_.Begin() + in_buf_.GetWriteIndex();
  vec[0].iov_len = in_buf_.GetWriteableSize();
  vec[1].iov_base = ext_buf;
  vec[1].iov_len = sizeof(ext_buf);
  const int vec_number = (in_buf_.GetWriteableSize() < sizeof(ext_buf)) ? 2 : 1;
  //  ssize_t n = socket_->Read(ext_buf, 64 * 1024);  // read from fd
  n = socket_->ReadV(vec, vec_number);  // read from fd
  if (n < 0) {
    int save_errno = errno;
    //    也就是说还没有占满预分配的vector
  } else if (n <= in_buf_.GetWriteableSize()) {
    in_buf_.SetWriteIndex(in_buf_.GetWriteIndex() + n);
  } else {
    in_buf_.SetWriteIndex(in_buf_.GetBufferSize());
    // FIXME while n - writeable > 65535
    in_buf_.Append(ext_buf, n - in_buf_.GetWriteableSize());
  }
#elif defined(OS_UNIX1)
  char ext_buf[65535];
  // TODO: copy or increase a system call？
  //  int can_write = in_buf_.GetWriteableSize();
  n = socket_->Read(ext_buf, 65535);  // read from fd
  if (n > 0) {
    in_buf_.Append(ext_buf, n);
  }
#endif
  // check
  if (n > 0) {
    // call msg callback
    on_message_(shared_from_this(), &in_buf_);
  } else if (n == 0) {
    log_trace("TcpEvent::HandleRead half Close", socket_->GetFd());
    HandleClose();
  } else {
    log_error("[TcpEvent::HandleRead]-> read <0 errno=%d errmsg = %s", errno,
              strerror(errno));
    HandleError();
  }
}
void TcpEvent::HandleWrite() {
  log_trace("TcpEvent::HandleWrite");
  if (event_->IsWriting()) {
    ssize_t n =
        socket_->Write((char*)out_buf_.Peek(), out_buf_.GetReadableSize());
    log_trace("write %d bytes to socket fd %d", n, socket_->GetFd());
    if (n > 0) {
      out_buf_.Retrieve(n);
      // Once the data is written, Close the write event immediately to avoid
      // busy loop
      if (out_buf_.GetReadableSize() == 0) {
        log_trace(
            "[TcpEvent::HandleWrite]->write done and try to stop writing");
        StopWriting();
        if (on_write_done_) {
          on_write_done_(shared_from_this());
        }
        // Ensure that the data in the buffer has been sent out.
        // Then shutdown the connection
        if (state_ == kDisconnecting) {
          TryEagerShutDown();
        }
      }
    }
  } else {
    log_error("TcpEvent::HandleWrite error");
  }
}
void TcpEvent::HandleClose() {
  assert(state_ == kConnected || state_ == kDisconnecting);
  SetState(kDisconnected);
  event_->DisableAll();
  // call user callback
  on_connection_(shared_from_this());
  if (on_close_) {
    //  close_callback();
    on_close_(shared_from_this());
  }
}
void TcpEvent::HandleError() {
  int err = socket_->GetSocketError();
  char buff[256]{};
  strerror_r(err, buff, sizeof buff);
  log_error("TcpEvent::HandleError SO_ERROR=%d msg=%s fd=%d", err, buff,
            socket_->GetFd());
}
TcpEvent::~TcpEvent() {
  log_debug("TcpEvent::~TcpEvent");
  assert(state_ == kDisconnected);
}

void TcpEvent::ConnectEstablished() {
  assert(state_ == kConnecting);
  SetState(kConnected);
  StartReading();

  // on connection open(accepted callback)
  if (on_connection_) {
    on_connection_(shared_from_this());
  }
}
void TcpEvent::ConnectDestroyed() {
  if (state_ == kConnected) {
    SetState(kDisconnected);
    // TODO FIXME:why TcpEvent::HandleClose() call event_->DisableAll();
    StopAll();
    // on connection Close(closed callback)
    if (on_connection_) {
      on_connection_(shared_from_this());
    }
  }
  // remove event from event map and  remove fd from pfds
  event_->UnRegister();
}
void TcpEvent::Send(std::string_view msg) { Send(msg.data(), msg.size()); }
void TcpEvent::Send(const char* data, size_t len) {
  if (state_ == kDisconnected) {
    log_warn("disconnected, give up writing");
    return;
  }
  size_t remaining = len;
  ssize_t n = 0;
  // If there is still data in the output buffer at this time,
  // it should not be sent directly, but the data is added to the buffer.
  if (out_buf_.GetReadableSize() == 0) {
    // FIXME test
    n = socket_->Write((char*)data, len);
    if (n >= 0) {
      // may be not write done
      remaining -= n;
      if (remaining == 0) {
        if (on_write_done_) {
          on_write_done_(shared_from_this());
        }
      }

    } else {
      n = 0;
      // Resource temporarily unavailable(EAGAIN)
      if (errno != EWOULDBLOCK) {
        log_error("TcpEvent::Send errno != EWOULDBLOCK");
      }
    }
  }

  // Put the unsent data into the output buffer and pay attention to the write
  // event
  if (remaining > 0) {
    // append remaining data to buffer
    out_buf_.Append(data + n, remaining);
    log_trace("TcpEvent::Send no more buffer enable writing...");
    StartWriting();
  }
}
void TcpEvent::Send(IoBuf* buffer) {
  Send(buffer->Peek(), buffer->GetReadableSize());
  buffer->Refresh();
}
void TcpEvent::ShutDown() {
  if (state_ == kConnected) {
    SetState(kDisconnecting);
    TryEagerShutDown();
  }
}
void TcpEvent::ForceClose() {
  if (state_ == kConnected || state_ == kDisconnecting) {
    SetState(kDisconnecting);
    // as if we received 0 byte in handleRead();
    HandleClose();
  }
}
void TcpEvent::TryEagerShutDown() {
  // we are not writing
  // 保证没有发送完毕的数据能够发送出去
  if (!event_->IsWriting()) {
    socket_->ShutDownWrite();
  }
}
