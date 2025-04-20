#include "runner.h"

#include <chrono>
#include <filesystem>
#include <random>
#include <system_error>
#include <tuple>

#include "runner_log.h"

namespace remote_agent {
Runner::Runner(const std::string &task_name)
    : _default_shell(true), _task_name(task_name) {
  createOutputName();
  register_log_file(_output_file);
}

CommandResult Runner::execute(const std::string &command) {
  boost::process::ipstream out_stream, err_stream;
  try {
    if (_default_shell) {
      BOOST_LOG_TRIVIAL(info)
          << ">> " << boost::process::shell().generic_string() << " -c \""
          << command << "\"";
      boost::process::child process(boost::process::shell(),
                                    boost::process::args = {"-c", command},
                                    boost::process::std_out > out_stream,
                                    boost::process::std_err > err_stream);
      auto res = readStream(process, out_stream, err_stream);
      BOOST_LOG_TRIVIAL(info)
          << boost::log::add_value(is_raw, true) << std::get<1>(res);
      BOOST_LOG_TRIVIAL(error)
          << boost::log::add_value(is_raw, true) << std::get<2>(res);

      return res;
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
    BOOST_LOG_TRIVIAL(info) << ">> " << shell << " -c \"" << command << "\"";
    boost::process::child process(boost::process::search_path(shell),
                                  boost::process::args = {"-c", command},
                                  boost::process::std_out > out_stream,
                                  boost::process::std_err > err_stream);
    auto res = readStream(process, out_stream, err_stream);
    BOOST_LOG_TRIVIAL(info)
        << boost::log::add_value(is_raw, true) << std::get<1>(res);
    BOOST_LOG_TRIVIAL(error)
        << boost::log::add_value(is_raw, true) << std::get<2>(res);

    return res;
  } catch (const std::system_error &e) {
    BOOST_LOG_TRIVIAL(fatal) << boost::log::add_value(is_raw, true) << e.what();
    return std::make_tuple(-1, "", e.what());
  } catch (const std::exception &e) {
    BOOST_LOG_TRIVIAL(fatal) << boost::log::add_value(is_raw, true) << e.what();
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

void Runner::createOutputName() {
  auto temp_dir = std::filesystem::temp_directory_path();
  auto seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::mt19937 generator(seed);
  _output_file =
      temp_dir / (_task_name + "_" + std::to_string(generator()) + ".txt");
}

std::string Runner::getOutputfile() { return _output_file; }
} // namespace remote_agent
