
#include <iostream>
#include <list>
#include <string>
#include <chrono>
#include <thread>
#include <syslog.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <ctime>

#include <boost/program_options.hpp>
#include <zmqpp/zmqpp.hpp>

#include "config.h"
#include "mail.h"
#include "runner.h"
#include "timer.h"

std::string getCurrentTimeStr() {

auto now = std::chrono::system_clock::now();
auto time = std::chrono::system_clock::to_time_t(now);
std::stringstream ss;
ss << std::put_time(std::localtime(&time), "%H:%M:%S");
return ss.str();
}

int main(int argc, char *argv[]) {
  // Example 1: One-shot timer
  remote_agent::Timer oneShot;
  std::cout << getCurrentTimeStr() << " - Starting one-shot timer for 2 seconds" << std::endl;
  oneShot.startOneShot(2000, []() {
      std::cout << getCurrentTimeStr() << " - One-shot timer fired!" << std::endl;
  });
  
  // Sleep to allow the one-shot timer to execute
  std::this_thread::sleep_for(std::chrono::seconds(3));

  // Example 2: Periodic timer
  remote_agent::Timer periodic;
  std::cout << getCurrentTimeStr() << " - Starting periodic timer with 1 second interval" << std::endl;
  periodic.startPeriodic(1000, []() {
      std::cout << getCurrentTimeStr() << " - Periodic timer fired!" << std::endl;
  });
  
  // Let the periodic timer run for 5 seconds
  std::this_thread::sleep_for(std::chrono::seconds(5));
  
  // Stop the periodic timer
  std::cout << getCurrentTimeStr() << " - Stopping periodic timer" << std::endl;
  periodic.stop();
  
  // Example 3: Timer with parameters
  remote_agent::Timer paramTimer;
  std::cout << getCurrentTimeStr() << " - Starting timer with parameters" << std::endl;
  int counter = 0;
  paramTimer.startPeriodic(500, [&counter](const std::string& message) {
      counter++;
      std::cout << getCurrentTimeStr() << " - " << message << " (count: " << counter << ")" << std::endl;
      if (counter >= 6) {
          std::cout << getCurrentTimeStr() << " - Reached target count" << std::endl;
      }
  }, "Parameter passed to timer");
  
  // Let it run for 3 seconds
  std::this_thread::sleep_for(std::chrono::seconds(3));
  
  // Stop all timers before exiting
  paramTimer.stop();
  
  std::cout << getCurrentTimeStr() << " - Program complete" << std::endl;
  return 0;
  const std::string endpoint = "tcp://*:5555";
  zmqpp::context context;
  zmqpp::socket_type type = zmqpp::socket_type::reply;
  zmqpp::socket socket (context, type);
  socket.bind(endpoint);
    while (1) {
      // receive the message
      zmqpp::message message;
      // decompose the message 
      socket.receive(message);
      std::string text;
      message >> text;
  
      //Do some 'work'
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::cout << "Received Hello" << std::endl;
      socket.send("World");
    }
  remote_agent::Runner runner;
  auto task = runner.parseTasks("/home/amin/code/amin/marialinux/remote_agent/build/Debug/task1.yaml");
  std::cout << runner.getOutputfile() << std::endl;
  for (const auto & step : task.steps) {
    std::cout << "Step: " << step.name << std::endl;
    for (const auto & env : step.environments) {
      std::cout << "Environment: " << env.first << "=" << env.second << std::endl;
    }
    for (const auto & command : step.commands) {
      std::cout << "Command: " << command << std::endl;
    }
  }
  runner.execute(task);
  return 0;
  try {
    openlog(argv[0], LOG_CONS | LOG_PID, LOG_USER);
    boost::program_options::options_description desc("Available options");
    desc.add_options()("help,h", "produce help message")
        // ("attachments,a",
        // boost::program_options::value<std::vector<std::string>>()->multitoken(),
        // "attachment files")
        ("config,c", boost::program_options::value<std::string>()->required(),
         "path to config file");
    boost::program_options::variables_map vm;
    boost::program_options::store(
        boost::program_options::command_line_parser(argc, argv)
            .options(desc)
            .run(),
        vm);
    if (vm.count("help")) {
      std::cout << "Usage: " << argv[0] << " [options]\n";
      std::cout << desc;
      return EXIT_SUCCESS;
    }
    boost::program_options::notify(vm);
    auto config_path = vm["config"].as<std::string>();
    std::cout << "Config file: " << config_path << std::endl;
    // auto attachments = vm["attachments"].as<std::vector<std::string>>();

    remote_agent::Config config =
        remote_agent::Config::getInstance(config_path);
    auto gmail = config.getAccountByName("gmail_account").value();

    remote_agent::mail::Mail mail(gmail);
    std::list<remote_agent::mail::File> file_list;
    // for (auto a : attachments) {
    //     std::ifstream ifs(a, std::ios::binary);

    //     remote_agent::mail::File file = make_tuple(std::ref(ifs),
    //     std::filesystem::path(a).filename().string(), "text/plain");
    //     file_list.push_back(file);
    // }
    // mail.send(u8"تست ایمیل با اتچمنت", u8"این یک ایمیل تست است.", file_list);
    mail.getByFilter();
  } catch (boost::program_options::required_option &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return EXIT_SUCCESS;
}
