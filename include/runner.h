#pragma once

#include <boost/process/v1/detail/child_decl.hpp>
#include <string>
#include <tuple>
#include <boost/process.hpp>
#include <optional>

#include "task.h"

namespace remote_agent {
using CommandResult = std::tuple<int, std::string, std::string>;
enum class Shell { SH, BASH, ZSH };

class Runner {
public:
  Runner(const std::string &task_name);
  Runner();
  CommandResult execute(const std::string &command);
  int execute(const Task &task, std::optional<std::string> error);
  void setShell(Shell shell);
  std::string getOutputfile();
  Task parseTasks(const std::string &yaml_file);

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
