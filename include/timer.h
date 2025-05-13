#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace remote_agent {
class Timer {
public:
  Timer();
  ~Timer();

  void stop();
  bool isRunning() const;

  template <typename Function, typename... Args>
  void startOneShot(int milliseconds, Function &&function, Args &&...args) {
    auto callback = std::make_shared<std::function<void()>>(std::bind(
        std::forward<Function>(function), std::forward<Args>(args)...));
    startTimer(milliseconds, false, callback);
  }

  template <typename Function, typename... Args>
  void startPeriodic(int milliseconds, Function &&function, Args &&...args) {
    auto callback = std::make_shared<std::function<void()>>(std::bind(
        std::forward<Function>(function), std::forward<Args>(args)...));

    startTimer(milliseconds, true, callback);
  }

private:
  void stopTimer();

  void startTimer(int milliseconds, bool periodic,
                  std::shared_ptr<std::function<void()>> callback);

  std::thread _thread;
  std::mutex _mutex;
  std::condition_variable _cv;
  std::atomic_bool _running;
};
} // namespace remote_agent
