#include "daemon.h"

#include <algorithm>
#include <iostream>
#include <regex>
#include <syslog.h>
#include <thread>

#include "config.h"
#include "mail.h"
#include "publisher.h"
#include "subscriber.h"

namespace remote_agent {

Daemon::Daemon() : _running(false) {
  _mail_enabled =
      (Config::getInstance().getGlobalConfig().check_mail_interval_ms > 0);
  _endpoint = Config::getInstance().getGlobalConfig().zeromq_endpoint;
  initServices();
  initSubscribers();
}

Daemon::~Daemon() {}

void Daemon::start() {
  if (_running) {
    return;
  }

  _running = true;
  std::thread t(&Daemon::run, this);
  t.join();
}

void Daemon::stop() {
  if (!_running) {
    return;
  }

  _running = false;
}

void Daemon::run() {
  startPublishService<std::string>();
  startSubscribeService<std::string>();
  if (_mail_enabled) {
    startMailService();
  }
  while (_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void Daemon::startMailService() {
  _mail_timer.startPeriodic(
      Config::getInstance().getGlobalConfig().check_mail_interval_ms, [this]() {
        for (auto &account : Config::getInstance().getAccounts()) {
          remote_agent::mail::Mail mail(account);
          auto [out_dir, err] = mail.getByFilter();
          if (err.has_value()) {
            std::cout << "Mail error: " << err.value().second << std::endl;
          }
          if (out_dir.empty()) {
            continue;
          }
          publishMail(out_dir);
        }
      });
}

const std::string Daemon::getSubscriberEndpoint(const std::string &endpoint) {
  std::regex pattern("tcp://(?:\\*|0\\.0\\.0\\.0)(:\\d+)");
  if (std::regex_match(endpoint, pattern))
    return std::regex_replace(endpoint, pattern, "tcp://localhost$1");
  return endpoint;
}

void Daemon::initServices() {
  _services[_endpoint + TOPIC_MAIL_RECV] =
      std::make_unique<ServiceContext<std::string>>(
          ServiceType::MAIL_RECV,
          Publisher<std::string>(_endpoint, TOPIC_MAIL_RECV));
}

void Daemon::initSubscribers() {
  _subscribers[_endpoint + TOPIC_MAIL_RECV] =
      std::make_unique<Subscriber<std::string>>(
          getSubscriberEndpoint(_endpoint), TOPIC_MAIL_RECV);
}

void Daemon::publishMail(const std::string& msg) {
  auto *service_context = dynamic_cast<ServiceContext<std::string> *>(
      _services[_endpoint + TOPIC_MAIL_RECV].get());
  std::unique_lock<std::mutex> lock(service_context->mutex);
  service_context->queue.push(msg);
  service_context->cv.notify_one();
}
} // namespace remote_agent
