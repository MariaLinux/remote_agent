#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

#include "publisher.h"
#include "subscriber.h"
#include "ipc.h"
#include "timer.h"

namespace remote_agent {

enum class ServiceType {
  MAIL_RECV = 0,
};

class ServiceContextBase {
public:
  virtual ~ServiceContextBase() = default;
};

template <typename Msg> struct ServiceContext : public ServiceContextBase {
  ServiceType type;
  std::mutex mutex;
  std::condition_variable cv;
  std::queue<Msg> queue;
  Publisher<Msg> publisher;
  std::thread thread;

  ServiceContext(ServiceType type, Publisher<Msg> publisher)
      : type(type), publisher(publisher) {}
};

constexpr char TOPIC_MAIL_RECV[] = "mail_recv";

class Daemon {
public:
  Daemon();
  ~Daemon();

  void start();
  void stop();

private:
  void run();
  void startMailService();
  void initServices();
  void initSubscribers();
  template <typename Msg> void startPublishService();
  template <typename Msg> void startSubscribeService();
  const std::string getSubscriberEndpoint(const std::string &endpoint);
  void publishMail(const std::string& mail);

  bool _mail_enabled;
  std::string _endpoint;
  remote_agent::Timer _mail_timer;
  std::atomic_bool _running;
  std::unordered_map<std::string, std::unique_ptr<ServiceContextBase>>
      _services;
  std::unordered_map<std::string, std::unique_ptr<IPC>> _subscribers;
};

template <typename Msg> void Daemon::startPublishService() {
  for (auto &[endpoint, service] : _services) {
    auto *service_context = dynamic_cast<ServiceContext<Msg> *>(service.get());
    if (service_context == nullptr) {
      continue;
    }
    service_context->publisher.connect();

    service_context->thread = std::thread([this, service_context]() {
      while (_running) {
        std::unique_lock<std::mutex> lock(service_context->mutex);
        service_context->cv.wait(lock, [this, service_context]() {
          return (!service_context->queue.empty() || !_running);
        });
        if (service_context->queue.empty()) {
          continue;
        }
        Msg msg = service_context->queue.front();
        service_context->queue.pop();
        lock.unlock();
        service_context->publisher.publish(msg);
      }
    });
  }
}


template <typename Msg> void Daemon::startSubscribeService() {
  auto* mail_recv_subscriber = dynamic_cast<Subscriber<Msg>*>(_subscribers[_endpoint + TOPIC_MAIL_RECV].get());
  if (mail_recv_subscriber != nullptr) {
    mail_recv_subscriber->connect();
    mail_recv_subscriber->subscribe([](const Msg& msg) {
      std::cout << "Mail recv: " << msg << std::endl;
    });
  }
}
} // namespace remote_agent
