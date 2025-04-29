#include "runner.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>
#include <system_error>
#include <tuple>

#include <yaml-cpp/yaml.h>
#include <boost/process/v1/environment.hpp>

#include "runner_log.h"

namespace remote_agent {
Runner::Runner(const std::string &task_name)
    : _default_shell(true), _task_name(task_name) {
  createOutputName();
  register_log_file(_output_file);
}

Runner::Runner()
    : _default_shell(true), _task_name("default_task") {
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
                                    // boost::this_process::environment(),
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
                                  // boost::this_process::environment(),
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

Task Runner::parseTasks(const std::string &yaml_file) {
  Task task;

    try {
      YAML::Node config = YAML::LoadFile(yaml_file);
      if (!config["name"]) {
        throw std::runtime_error("Missing 'name' field in YAML file.");
      }
      task.name = config["name"].as<std::string>();

      if (!config["steps"] || !config["steps"].IsSequence()) {
        throw std::runtime_error("'steps' field is missing or not a sequence in YAML file.");
      }
      for (const auto &step : config["steps"]) {
        if (!step["name"]) {
          throw std::runtime_error("A step is missing the 'name' field in YAML file.");
        }
        Step step_obj;
        step_obj.name = step["name"].as<std::string>();

        if (!step["commands"] || !step["commands"].IsSequence()) {
          throw std::runtime_error("The 'commands' field is missing or not a sequence in a step.");
        }
        for (const auto &command : step["commands"]) {
          step_obj.commands.push_back(command.as<std::string>());
        }

        if (step["environments"] && step["environments"].IsSequence()) {
          for (const auto &env : step["environments"]) {
            if (env.IsMap()){
              for (const auto &env_pair : env) {
                step_obj.environments[env_pair.first.as<std::string>()] = env_pair.second.as<std::string>();
              }
            }
          }
        }
        task.steps.push_back(step_obj);
      }
    } catch (const std::exception &e) {
      BOOST_LOG_TRIVIAL(fatal) << "Error parsing YAML file: " << e.what();
    }
  
  _task_name = task.name;
  createOutputName();
  register_log_file(_output_file);
  
  return task;
}

int Runner::execute(const Task &task) {
  for (const auto & step : task.steps) {
    BOOST_LOG_TRIVIAL(info) << "-- Executing step: " << step.name;
    // Set up the environment for the step commands
    for (const auto &env : step.environments) {
      std::cout << "Setting environment variable: " << env.first
                << " = " << env.second << std::endl;
      boost::this_process::environment()[env.first] = env.second;

    }
    // Execute the commands in the step
    for (const auto & command : step.commands) {
      const auto [exit_code, output, error] = execute(command);
      if (exit_code != 0)
        return exit_code;
    }
    // Clean up the environment
    for (const auto &env : step.environments) {
      std::cout << "Removing environment variable: " << env.first << std::endl;
      boost::this_process::environment().erase(env.first);
    }
  }
  return 0;
}

} // namespace remote_agent
