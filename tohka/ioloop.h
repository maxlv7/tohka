//
// Created by li on 2022/2/17.
//

#ifndef TOHKA_TOHKA_IOLOOP_H
#define TOHKA_TOHKA_IOLOOP_H

#include <memory>

#include "ioevent.h"
#include "iowatcher.h"
#include "poll.h"
#include "timepoint.h"
#include "timermanager.h"

namespace tohka {
class IoLoop : public noncopyable {
 public:
  using TimerTask = std::function<void()>;
  IoLoop();

  ~IoLoop() = default;
  void RunForever();
  void Quit() { running_ = false; };

  //  void CallSoon();
  void CallAt(TimePoint when, TimerTask callback);
  void CallLater(int delay, TimerTask callback);
  void CallEvery(int interval, TimerTask callback);

  IoWatcher* GetPoint();

 private:
  using IoWatcherPtr = std::unique_ptr<IoWatcher>;
  using TimerManagerPtr = std::unique_ptr<TimerManager>;
  IoWatcherPtr io_watcher_;
  TimerManagerPtr timer_manager_;

  bool running_;
};
}  // namespace tohka
#endif  // TOHKA_TOHKA_IOLOOP_H
