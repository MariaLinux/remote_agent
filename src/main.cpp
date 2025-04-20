#include <algorithm>
#include <boost/program_options.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <syslog.h>

#include "config.h"
#include "mail.h"
#include "runner.h"

int main(int argc, char *argv[]) {
  remote_agent::Runner runner("task");
  std::cout << runner.getOutputfile() << std::endl;
  const auto [exit_code, output, error] = runner.execute("pstree");
  if (exit_code == 0) {
    std::cout << "output: " << output << std::endl;
    std::cerr << "error: " << error << std::endl;
  } else {
    std::cerr << "error: " << error << std::endl;
  }
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
