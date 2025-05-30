#pragma once

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

#include <zmqpp/message.hpp>

#include "ipc.h"
#include "publisher.h"
#include "subscriber.h"
#include "timer.h"
#include "mail_to.pb.h"


namespace remote_agent {

enum class ServiceType {
  MAIL_RECV = 0,
  TASK_RECV = 1,
  MAIL_SEND = 2,
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
constexpr char TOPIC_TASK_RECV[] = "task_recv";
constexpr char TOPIC_MAIL_SEND[] = "mail_send";

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
  void processMail(const std::string &mail_dir);
  void sendMail(const MailTo& info);
  template <typename Msg>
  void publish(const Msg &msg, const std::string &topic);
  void processTask(const std::string &task_file);

  bool _mail_enabled;
  std::string _endpoint;
  remote_agent::Timer _mail_timer;
  std::atomic_bool _running;
  std::unordered_map<std::string, std::unique_ptr<ServiceContextBase>>
      _services;
  std::unordered_map<std::string, std::unique_ptr<IPC>> _subscribers;
  std::mutex _mail_checker_mutex;
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
        std::cout << "Publish msg to topic: " <<  typeid(msg).name() <<
        std::endl;
        service_context->publisher.publish(msg);
      }
    });
  }
}

template <typename Msg> void Daemon::startSubscribeService() {
  auto *mail_recv_subscriber = dynamic_cast<Subscriber<Msg> *>(
      _subscribers[_endpoint + TOPIC_MAIL_RECV].get());
  if (mail_recv_subscriber != nullptr) {
    mail_recv_subscriber->connect();
    mail_recv_subscriber->subscribe([this](const Msg &msg) {
      if constexpr (std::is_same_v<Msg, std::string>) {
        std::cout << "Mail recv: " << msg << std::endl;
        processMail(msg);
      }
    });
  }
  auto *task_recv_subscriber = dynamic_cast<Subscriber<Msg> *>(
      _subscribers[_endpoint + TOPIC_TASK_RECV].get());
  if (task_recv_subscriber != nullptr) {
    task_recv_subscriber->connect();
    task_recv_subscriber->subscribe([this](const Msg &msg) {
      if constexpr (std::is_same_v<Msg, std::string>) {
        std::cout << "Task recv: " << msg << std::endl;
        processTask(msg);
      }
    });
  }
  auto *mail_send_subscriber = dynamic_cast<Subscriber<Msg> *>(
      _subscribers[_endpoint + TOPIC_MAIL_SEND].get());
  if (mail_send_subscriber != nullptr) {
    mail_send_subscriber->connect();
    mail_send_subscriber->subscribe([this](const Msg& msg) {
      std::cout << "Mail send" << std::endl;
      if constexpr (std::is_same_v<Msg, std::string>) {
        MailTo msg_to_send;
        if (!msg_to_send.ParseFromString(msg)) {
          std::cout << "Error parsing MailTo" << std::endl;
          return;
        }
        std::cout << "Mail send: " << msg_to_send.subject() << std::endl;
        sendMail(msg_to_send);
      }
    });
  }
}

template <typename Msg>
void Daemon::publish(const Msg &msg, const std::string &topic) {
  std::cout<< __func__ <<" Publish msg to topic: " << topic << std::endl;
  auto* service_context =
      dynamic_cast<ServiceContext<Msg> *>(_services[_endpoint + topic].get());
  std::unique_lock<std::mutex> lock(service_context->mutex);
  service_context->queue.push(msg);
  service_context->cv.notify_one();
}
} // namespace remote_agent
