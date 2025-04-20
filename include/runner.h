#pragma once

#include <boost/process/v1/detail/child_decl.hpp>
#include <string>
#include <tuple>

#include <boost/process.hpp>

namespace remote_agent {
using CommandResult = std::tuple<int, std::string, std::string>;
enum class Shell { SH, BASH, ZSH };

class Runner {
public:
  Runner(const std::string &task_name);
  CommandResult execute(const std::string &command);
  void setShell(Shell shell);
  std::string getOutputfile();

private:
  CommandResult readStream(boost::process::child &process,
                           boost::process::ipstream &out_stream,
                           boost::process::ipstream &err_stream);
  void createOutputName();

  bool _default_shell;
  Shell _shell;
  std::string _task_name;
  std::string _output_file;
};
} // namespace remote_agent
