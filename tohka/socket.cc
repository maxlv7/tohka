//
// Created by li on 2022/2/27.
//

#include "socket.h"

#include "util/log.h"
using namespace tohka;

Socket::Socket(int domain, int type, int protocol) {
  int sock = ::socket(domain, type, protocol);
  if (sock < 0) {
    log_error("create socket error! errno=%d errstr = %s", errno,
              strerror(errno));
  }
  fd_ = sock;
}

Socket::Socket(int fd) : fd_(fd) {}

Socket::~Socket() {
  // Close socket
  log_debug("Socket::~Socket");
  Close();
}
void Socket::BindAddress(NetAddress& address) const {
  int ret = ::bind(fd_, address.GetAddress(), address.GetSize());
  if (ret < 0) {
    log_error("bind error! errno=%d errstr = %s", errno, strerror(errno));
  }
}

void Socket::Listen() const {
  int ret = ::listen(fd_, kBacklog);
  if (ret < 0) {
    log_error("listen error! errno=%d errstr = %s", errno, strerror(errno));
  }
}

int Socket::Accept(NetAddress* peer_address) const {
  struct sockaddr_in6 socket_address6 {};
  ::memset(&socket_address6, 0, sizeof(socket_address6));
  socklen_t sock_len = sizeof(socket_address6);

  int conn_fd = ::accept(fd_, (sockaddr*)&socket_address6, &sock_len);
  Socket::SetNonBlockAndCloseOnExec(conn_fd);
  // TODO For debug only
  //  int opt = 3;
  //  if (::setsockopt(conn_fd, SOL_SOCKET, SO_SNDBUF, &opt,
  //                   (socklen_t)(sizeof(opt))) < 0) {
  //    log_error("SocketFd SetSO_SNDBUF error");
  //  }

  if (conn_fd < 0) {
    log_error("[Socket::Accept]->accept error! errno=%d errstr = %s", errno,
              strerror(errno));
  }
  peer_address->SetSockAddrInet6(socket_address6);
  return conn_fd;
}

int Socket::Connect(NetAddress& peer_address) const {
  socklen_t sock_len = peer_address.GetSize();
  return ::connect(fd_, peer_address.GetAddress(), sock_len);
}

void Socket::SetTcpNoDelay(bool on) const {
  int opt = on ? 1 : 0;
  if (::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt,
                   (socklen_t)(sizeof(opt))) < 0) {
    log_error("SocketFd SetReusePort error");
  }
}

void Socket::SetReuseAddress(bool on) const {
  int opt = on ? 1 : 0;
  if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt,
                   (socklen_t)(sizeof(opt))) < 0) {
    log_error("Socket::SetReuseAddress SetReuseAddress error");
  }
}

void Socket::SetReusePort(bool on) const {
  int opt = on ? 1 : 0;
  if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &opt,
                   (socklen_t)(sizeof(opt))) < 0) {
    log_error("Socket::SetReuseAddress SetReusePort error");
  }
}

void Socket::SetKeepAlive(bool on) const {
  int opt = on ? 1 : 0;
  if (::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &opt,
                   (socklen_t)(sizeof(opt))) < 0) {
    log_error("Socket::SetKeepAlive SetKeepAlive error");
  }
}

ssize_t Socket::Read(void* buffer, size_t len) const {
  return ::read(fd_, buffer, len);
}
ssize_t Socket::Write(void* buffer, size_t len) const {
  return ::write(fd_, buffer, len);
}

void Socket::SetNonBlockAndCloseOnExec(int sock_fd) {
  // non-block
#ifdef OS_UNIX
  int flags = ::fcntl(sock_fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int ret = ::fcntl(sock_fd, F_SETFL, flags);
  if (ret < 0) {
    log_error("Socket::SetNonBlockAndCloseOnExec [non-block] error!");
  }

  // Close-on-exec
  flags = ::fcntl(sock_fd, F_GETFD, 0);
  flags |= FD_CLOEXEC;
  ret = ::fcntl(sock_fd, F_SETFD, flags);
  if (ret < 0) {
    log_error("Socket::SetNonBlockAndCloseOnExec [Close-on-exec] error!");
  }

#elif OS_WIN
  // TODO win32
#endif
}

int Socket::CreateNonBlockFd(int domain, int type, int protocol) {
  int sock = ::socket(domain, type, protocol);
  if (sock < 0) {
    log_error("create socket error! errno=%d errstr = %s", errno,
              strerror(errno));
  }
  SetNonBlockAndCloseOnExec(sock);
  return sock;
}
void Socket::Close() {
  if (!closed) {
    ::close(fd_);
    closed = true;
  }
}
int Socket::GetSocketError() const {
  int opt_val;
  auto opt_len = static_cast<socklen_t>(sizeof opt_val);

  if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &opt_val, &opt_len) < 0) {
    return errno;
  } else {
    return opt_val;
  }
}
void Socket::ShutDownWrite() const {
  if (::shutdown(fd_, SHUT_WR) < 0) {
    log_error("[Socket::ShutDownWrite]->shutdownWrite fd=%d", fd_);
  }
}

int Socket::GetPeerName(NetAddress& peer) const {
  socklen_t len = peer.GetSize();
  if (::getpeername(fd_, peer.GetAddress(), &len) < 0) {
    log_error("[ Socket::GetPeerName] error fd = %d errmsg = %s", fd_,
              strerror(errno));
    return -1;
  } else {
    return 0;
  }
}

#ifdef OS_UNIX
ssize_t Socket::ReadV(const struct iovec* vec, int len) const {
  return ::readv(fd_, vec, len);
}
#endif
