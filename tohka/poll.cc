//
// Created by li on 2022/2/24.
//

#include "poll.h"

#include "ioevent.h"
#include "timepoint.h"
#include "tohka/tohka.h"
#include "util/log.h"

using namespace tohka;

Poll::Poll() { pfds_.reserve(kInitialSize); }

TimePoint Poll::PollEvents(int timeout, EventList* event_list) {
  // 调用poll 并构造活动的事件
  //  log_info("poll has %d events",pfds_.size());
  //  for (const auto& item : pfds_){
  //    log_info("fd = %d events=%d
  //    revents=%d",item.fd,item.events,item.revents);
  //  }
  int active_events = poll(pfds_.data(), pfds_.size(), timeout);
  if (active_events > 0) {
    log_trace("Poll::PollEvents %d events happened", active_events);
    for (const auto pfd : pfds_) {
      if (pfd.revents > 0) {
        auto it = io_events_map.find(pfd.fd);
        assert(io_events_map.size() == pfds_.size());
        assert(it != io_events_map.end());
        IoEvent* event = it->second;

        assert(pfd.fd == event->GetFd());

        short what = pfd.revents;
        short res = 0;
        //        ReventsToString(what);
        if (what & (POLLHUP | POLLERR | POLLNVAL)) {
          what |= POLLIN | POLLOUT;
        }
        if (what & POLLIN) {
          res |= EV_READ;
        }
        if (what & POLLOUT) {
          res |= EV_WRITE;
        }
        event->SetRevents(res);
        event_list->emplace_back(event);
        --active_events;
        if (active_events == 0) {
          break;
        }
      }
    }
  } else if (active_events == 0) {
    log_trace("Poll::PollEvents nothing io events happened!");
    log_debug("[Poll::PollEvents]->now have %d events in loop", pfds_.size());
  } else {
    log_error("Poll::PollEvents error while poll... errno=%d errmsg=%s", errno,
              strerror(errno));
  }

  return TimePoint::now();
}

void Poll::RegisterEvent(IoEvent* io_event) {
  // There are two situations: one is to add new event,
  // and the other is to update existing event
  if (io_event->GetIndex() == -1) {
    // 保证这个fd不存在io_events_map
    if (io_events_map.find(io_event->GetFd()) != io_events_map.end()) {
      log_error("RegisterEvent,fd = %d", io_event->GetFd());
    }
    assert(io_events_map.find(io_event->GetFd()) == io_events_map.end());
    struct pollfd pfd {};
    pfd.events = 0;
    pfd.revents = 0;
    pfd.fd = io_event->GetFd();
    switch (io_event->GetEvents()) {
      case EV_READ:
        pfd.events |= POLLIN;
        break;
      case EV_WRITE:
        pfd.events |= POLLOUT;
        break;
    }
    pfds_.emplace_back(pfd);

    // why -1
    // before SetIndex pdf was pushed to vector
    io_event->SetIndex((int)pfds_.size() - 1);
    io_events_map.emplace(io_event->GetFd(), io_event);
    log_trace("Poll::RegisterEvent new event:fd = %d events=%d revents=%d",
              io_event->GetFd(), io_event->GetEvents(), io_event->GetRevents());
  } else {
    assert(io_events_map.find(io_event->GetFd()) != io_events_map.end());
    assert(io_events_map[io_event->GetFd()] == io_event);

    int index = io_event->GetIndex();
    assert(index >= 0 && index < pfds_.size());
    auto& pfd = pfds_.at(index);
    assert(pfd.fd == io_event->GetFd() || pfd.fd == -io_event->GetFd() - 1);
    short events = io_event->GetEvents();
    pfd.fd = io_event->GetFd();
    pfd.events = EV_NONE;
    pfd.revents = EV_NONE;
    if (events & EV_READ) {
      pfd.events |= POLLIN;
    }
    if (events & EV_WRITE) {
      pfd.events |= POLLOUT;
    }
    if (pfd.events == EV_NONE) {
      pfd.fd = -io_event->GetFd() - 1;
    }
    log_trace(
        "Poll::RegisterEvent update event: fd = %d event:events=0x%x pfd "
        "events=0x%x",
        io_event->GetFd(), io_event->GetEvents(), pfd.events);
  }
}

void Poll::UnRegisterEvent(IoEvent* io_event) {
  auto it = io_events_map.find(io_event->GetFd());
  if (it != io_events_map.end()) {
    int fd = it->first;

    assert(io_events_map.find(io_event->GetFd()) != io_events_map.end());
    assert(fd == it->second->GetFd());
    assert(io_events_map[fd] == io_event);
    assert(io_event->GetEvents() == EV_NONE);

    assert((-io_event->GetFd() - 1) == pfds_[io_event->GetIndex()].fd);

    int idx = io_event->GetIndex();
    assert(idx >= 0 && idx < pfds_.size());
    const auto& pfd = pfds_[idx];
    assert(pfd.fd == -io_event->GetFd() - 1 &&
           pfd.events == io_event->GetEvents());
    size_t n = io_events_map.erase(io_event->GetFd());
    assert(n == 1);
    (void)n;
    if (idx == pfds_.size() - 1) {
      log_trace("Poll::UnRegisterEvent remove fd = %d back = %d", fd,
                pfds_.back());
      pfds_.pop_back();
    } else {
      // remove fd from pfds( O(1) )
      // 3 4 5 6 7 8 fd
      // 0 1 2 3 4 5 idx
      int end_fd = pfds_.back().fd;
      std::iter_swap(pfds_.begin() + idx, pfds_.end() - 1);
      if (end_fd < 0) {
        end_fd = -end_fd - 1;
      }
      // change idx
      io_events_map[end_fd]->SetIndex(idx);
      log_trace("Poll::UnRegisterEvent remove fd = %d end fd = %d back = %d",
                fd, end_fd, pfds_.back());
      pfds_.pop_back();
    }
  } else {
    // warning
    log_warn("can not find event fd=%d", io_event->GetFd());
  }
}
void Poll::ReventsToString(short revent) {
  std::string info = "[";
  if (revent & POLLIN) {
    info += "POLLIN|";
  }
  if (revent & POLLOUT) {
    info += "POLLOUT|";
  }
  if (revent & POLLHUP) {
    info += "POLLHUP|";
  }
  if (revent & POLLERR) {
    info += "POLLERR|";
  }
  if (revent & POLLNVAL) {
    info += "POLLNVAL|";
  }
  info += "]";
  log_debug("revents=%s", info.c_str());
}
