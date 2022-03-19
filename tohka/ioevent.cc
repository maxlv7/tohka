//
// Created by li on 2022/2/20.
//

#include "ioevent.h"

#include "util/log.h"

using namespace tohka;

IoEvent::IoEvent(IoWatcher* io_watcher, int fd)
    : io_watcher_(io_watcher), fd_(fd), events_(0), revents_(0), index_(-1) {}

// TODO Determined by the operating system
#ifdef OS_UNIX
#include "sys/poll.h"
void IoEvent::ExecuteEvent() {
  log_trace("fd = %d IoEvent::ExecuteEvent() revents=0x%x", fd_, revents_);
  // Hung up and no data to read
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
    log_warn("IoEvent::ExecuteEvent() hung up");
    if (close_callback_) {
      close_callback_();
    };
  }
  // fd is not an opened file
  if (revents_ & POLLNVAL) {
    log_warn("fd=%d IoEvent::ExecuteEvent() POLLNVAL", fd_);
  }
  // Invalid request and error condition
  if (revents_ & (POLLERR | POLLNVAL)) {
    if (error_callback_) {
      log_trace("IoEvent::ExecuteEvent() execute error_callback_");
      error_callback_();
    };
  }
  // Readable, urgent (read) and half-closed
  if (revents_ & (POLLIN | POLLPRI | POLLHUP)) {
    if (read_callback_) {
      log_trace("IoEvent::ExecuteEvent() execute read_callback_");
      read_callback_();
    };
  }
  if (revents_ & POLLOUT) {
    if (write_callback_) {
      log_trace("IoEvent::ExecuteEvent() execute write_callback_");
      write_callback_();
    };
  }
}
#endif
void IoEvent::Register() { io_watcher_->RegisterEvent(this); }
void IoEvent::UnRegister() { io_watcher_->UnRegisterEvent(this); }
