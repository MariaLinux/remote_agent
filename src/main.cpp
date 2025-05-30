#include <ctime>
#include <iostream>
#include <string>
#include <syslog.h>

#include <boost/program_options.hpp>

#include "config.h"
#include "daemon.h"


int main(int argc, char *argv[]) {
  try {
    openlog(argv[0], LOG_CONS | LOG_PID, LOG_USER);
    boost::program_options::options_description desc("Available options");
    desc.add_options()("help,h", "produce help message")
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
    remote_agent::Config config =
        remote_agent::Config::getInstance(config_path);
  } catch (boost::program_options::required_option &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  remote_agent::Daemon daemon;
  daemon.start();
  
  return EXIT_SUCCESS;
}
