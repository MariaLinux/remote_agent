#include "runner.h"

#include <boost/process/v1/search_path.hpp>
#include <system_error>
#include <tuple>

namespace remote_agent {
Runner::Runner() : _default_shell(true) {}

CommandResult Runner::execute(const std::string &command) {
  boost::process::ipstream out_stream, err_stream;

  try {
    if (_default_shell) {
      boost::process::child process(boost::process::shell(),
                                    boost::process::args = {"-c", command},
                                    boost::process::std_out > out_stream,
                                    boost::process::std_err > err_stream);
      return readStream(process, out_stream, err_stream);
    }
    std::string shell;
    switch (_shell) {
    case Shell::ZSH:
      shell = "zsh";
      break;
    case Shell::BASH:
      shell = "bash";
      break;
    case Shell::SH:
    default:
      shell = "sh";
      break;
    }

    boost::process::child process(boost::process::search_path(shell),
                                  boost::process::args = {"-c", command},
                                  boost::process::std_out > out_stream,
                                  boost::process::std_err > err_stream);
    return readStream(process, out_stream, err_stream);
  } catch (const std::system_error &e) {
    return std::make_tuple(-1, "", e.what());
  } catch (const std::exception &e) {
    return std::make_tuple(-1, "", e.what());
  }
}

void Runner::setShell(Shell shell) {
  _default_shell = false;
  _shell = shell;
}

CommandResult Runner::readStream(boost::process::child &process,
                                 boost::process::ipstream &out_stream,
                                 boost::process::ipstream &err_stream) {
  std::string stdout_content;
  std::string stderr_content;
  std::string line;
  while (out_stream && std::getline(out_stream, line)) {
    stdout_content += line + "\n";
  }

  while (err_stream && std::getline(err_stream, line)) {
    stderr_content += line + "\n";
  }

  process.wait();
  int exit_code = process.exit_code();

  return std::make_tuple(exit_code, stdout_content, stderr_content);
}

} // namespace remote_agent
