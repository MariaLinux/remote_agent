#include "daemon.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <syslog.h>
#include <thread>

#include "config.h"
#include "file_utils.h"
#include "mail.h"
#include "mail_to.pb.h"
#include "publisher.h"
#include "runner.h"
#include "subscriber.h"
#include "task.h"

namespace remote_agent {

Daemon::Daemon() : _running(false) {
  _mail_enabled =
      (Config::getInstance().getGlobalConfig().check_mail_interval_ms > 0);
  std::cout << "check_mail_interval_ms: "
            << Config::getInstance().getGlobalConfig().check_mail_interval_ms
            << " " << _mail_enabled << std::endl;
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
  // startPublishService<MailInfo>();
  startSubscribeService<std::string>();
  // startSubscribeService<MailInfo>();
  if (_mail_enabled) {
    startMailService();
  }
  while (_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void Daemon::startMailService() {
  std::cout << __FUNCTION__ << std::endl;
  _mail_timer.startPeriodic(
      Config::getInstance().getGlobalConfig().check_mail_interval_ms, [this]() {
        std::cout << "periodic mail checker" << std::endl;
        std::unique_lock<std::mutex> lock(_mail_checker_mutex,
                                          std::try_to_lock);
        if (!lock.owns_lock()) {
          return;
        }
        // auto account =
        // Config::getInstance().getAccountByName("gmail_account").value();
        for (const auto &account : Config::getInstance().getAccounts()) {
          std::cout << "account name:" << account.name << std::endl;
          remote_agent::mail::Mail mail(account);
          auto [out_dir, err] = mail.getByFilter();
          if (err.has_value()) {
            std::cout << "Mail error: " << err.value().second << std::endl;
            continue;
          }
          if (out_dir.empty()) {
            continue;
          }
          publish<std::string>(out_dir, TOPIC_MAIL_RECV);
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
  _services[_endpoint + TOPIC_TASK_RECV] =
      std::make_unique<ServiceContext<std::string>>(
          ServiceType::TASK_RECV,
          Publisher<std::string>(_endpoint, TOPIC_TASK_RECV));
  _services[_endpoint + TOPIC_MAIL_SEND] =
      std::make_unique<ServiceContext<std::string>>(
          ServiceType::MAIL_SEND,
          Publisher<std::string>(_endpoint, TOPIC_MAIL_SEND));
  // _services[_endpoint + TOPIC_MAIL_SEND] =
  //     std::make_unique<ServiceContext<MailInfo>>(
  //         ServiceType::MAIL_SEND,
  //         Publisher<MailInfo>(_endpoint, TOPIC_MAIL_SEND));
}

void Daemon::initSubscribers() {
  _subscribers[_endpoint + TOPIC_MAIL_RECV] =
      std::make_unique<Subscriber<std::string>>(
          getSubscriberEndpoint(_endpoint), TOPIC_MAIL_RECV);
  _subscribers[_endpoint + TOPIC_TASK_RECV] =
      std::make_unique<Subscriber<std::string>>(
          getSubscriberEndpoint(_endpoint), TOPIC_TASK_RECV);
  _subscribers[_endpoint + TOPIC_MAIL_SEND] =
      std::make_unique<Subscriber<std::string>>(getSubscriberEndpoint(_endpoint),
                                             TOPIC_MAIL_SEND);
  // _subscribers[_endpoint + TOPIC_MAIL_SEND] =
  //     std::make_unique<Subscriber<MailInfo>>(getSubscriberEndpoint(_endpoint),
  //                                            TOPIC_MAIL_SEND);
}

void Daemon::processMail(const std::string &mail_dir) {
  std::cout << "mail dir: " << mail_dir << std::endl;
  try {
    if (std::filesystem::exists(mail_dir) &&
        std::filesystem::is_directory(mail_dir)) {
      for (const auto &entry : std::filesystem::directory_iterator(mail_dir)) {
        std::cout << entry.path().filename() << std::endl;
        if (entry.path().extension() == ".zip") {
          auto out_dir = std::filesystem::path(mail_dir) / entry.path().stem();
          if (std::filesystem::create_directories(out_dir)) {
            Zip zip;
            auto err = zip.extract(entry.path().string(), out_dir.string());
            if (err.has_value()) {
              std::cout << "Zip error: " << err.value().second << std::endl;
            } else {
              for (const auto &file :
                   std::filesystem::directory_iterator(out_dir)) {
                std::cout << file.path().filename() << std::endl;
                if (file.path().extension() == ".yaml" ||
                    file.path().extension() == ".yml") {
                  publish<std::string>(file.path().string(), TOPIC_TASK_RECV);
                }
              }
            }
          }
        } else if (entry.path().extension() == ".yaml" ||
                   entry.path().extension() == ".yml") {
          publish<std::string>(entry.path().string(), TOPIC_TASK_RECV);
        }
        std::cout << "extension: " << entry.path().extension() << std::endl;
      }
    }
  } catch (const std::filesystem::filesystem_error &e) {
    std::cerr << "Filesystem error: " << e.what() << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}
void Daemon::processTask(const std::string &task_file) {
  std::cout << "task file: " << task_file << std::endl;
  TaskParser task_parser;
  if (!task_parser.parseYaml(task_file)) {
    std::cout << "Error parsing task file: " << task_parser.getError().value()
              << std::endl;
    return;
  }
  auto task = task_parser.getTask();
  Runner runner;
  auto res = runner.execute(task, task_parser.getError());
  std::cout << "Result: " << res << std::endl;
  MailTo msg_to_send;
  msg_to_send.set_subject(task.name);
  msg_to_send.set_body((res == 0) ? "Task completed successfully" : "Task failed");
  auto* attachment = msg_to_send.add_file_list();
  attachment->set_local_filepath(runner.getOutputfile());
  attachment->set_mime_type("text/plain");
  // MailInfo info;
  // info.subject = task.name;
  // info.body = (res == 0) ? "Task completed successfully" : "Task failed";
  // info.file_list.push_back(make_pair(runner.getOutputfile(), "text/plain"));
  publish<std::string>(msg_to_send.SerializeAsString(), TOPIC_MAIL_SEND);
}

void Daemon::sendMail(const MailTo& info) {
  std::cout << "Sending mail" << std::endl;
  std::cout << "Subject: " << info.subject() << std::endl;
  std::cout << "Body: " << info.body() << std::endl;
  std::cout << "File: " << info.file_list(0).local_filepath() << std::endl;
  std::list<mail::File> file_list;
  bool found = false;
  for (const auto &f : info.file_list()) {
    std::shared_ptr<std::ifstream> ifs = std::make_shared<std::ifstream>(f.local_filepath(), std::ios::binary);
    std::cout << "File: " << f.local_filepath() <<" MIME: " << f.mime_type() << std::endl; 
    mail::File file = std::make_pair(f.local_filepath(), f.mime_type());
    file_list.push_back(file);
  }
  for (const auto &account : Config::getInstance().getAccounts()) {
    if (info.has_account_name() &&
        info.account_name() != account.name) {
      continue;
    }
    found = true;
    remote_agent::mail::Mail mail(account);

    if (file_list.empty()) {
      auto err = mail.send(info.subject(), info.body());
      if (err.has_value()) {
        std::cout << "Mail error: " << err.value().second << std::endl;
      }
    } else {
      auto err = mail.send(info.subject(), info.body(), file_list);
      if (err.has_value()) {
        std::cout << "Mail error: " << err.value().second << std::endl;
      }
    }
  }
  if (!found) {
    std::cout << "No account found for sending mail (" << info.account_name() << ")" << std::endl;
  }
}
} // namespace remote_agent
