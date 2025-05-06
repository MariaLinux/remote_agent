#include "timer.h"

#include <chrono>

namespace remote_agent {

Timer::Timer() : _running(false) {}
Timer::~Timer() { stopTimer(); }

void Timer::stop() { stopTimer(); }

bool Timer::isRunning() const { return _running.load(); }

void Timer::stopTimer() {
  bool expected = true;
  // Only try to stop if currently running (avoid multiple threads trying to
  // join)
  if (_running.compare_exchange_strong(expected, false)) {
    _cv.notify_all();
    if (_thread.joinable()) {
      _thread.join();
    }
  }
}

void Timer::startTimer(int milliseconds, bool periodic,
                       std::shared_ptr<std::function<void()>> callback) {
  stopTimer();

  _running = true;

  _thread = std::thread([this, milliseconds, periodic, callback]() {
    auto interval = std::chrono::milliseconds(milliseconds);
    std::unique_lock<std::mutex> lock(_mutex);

    while (_running) {
      // Wait for the duration or until stopped
      auto status = _cv.wait_for(lock, interval);

      if (!_running) {
        break;
      }

      // If timeout occurred (rather than being notified), execute callback
      if (status == std::cv_status::timeout) {
        lock.unlock();
        (*callback)();
        lock.lock();

        // If this is a one-shot timer, exit after one execution
        if (!periodic) {
          _running = false;
          break;
        }
      }
    }
  });
}
} // namespace remote_agent
