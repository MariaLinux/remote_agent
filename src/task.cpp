#include "task.h"

#include <optional>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace remote_agent {

TaskParser::TaskParser() {}

bool TaskParser::parseYaml(std::string filename) {
  _error = std::nullopt;
  try {
    YAML::Node config = YAML::LoadFile(filename);
    if (!config["name"]) {
      _task.name = "Unnamed_task";
      throw std::runtime_error("Missing 'name' field in YAML file.");
    }
    _task.name = config["name"].as<std::string>();

    if (!config["steps"] || !config["steps"].IsSequence()) {
      throw std::runtime_error(
          "'steps' field is missing or not a sequence in YAML file.");
    }
    for (const auto &step : config["steps"]) {
      if (!step["name"]) {
        throw std::runtime_error(
            "A step is missing the 'name' field in YAML file.");
      }
      Step step_obj;
      step_obj.name = step["name"].as<std::string>();

      if (!step["commands"] || !step["commands"].IsSequence()) {
        throw std::runtime_error(
            "The 'commands' field is missing or not a sequence in a step.");
      }
      for (const auto &command : step["commands"]) {
        step_obj.commands.push_back(command.as<std::string>());
      }

      if (step["environments"] && step["environments"].IsSequence()) {
        for (const auto &env : step["environments"]) {
          if (env.IsMap()) {
            for (const auto &env_pair : env) {
              step_obj.environments[env_pair.first.as<std::string>()] =
                  env_pair.second.as<std::string>();
            }
          }
        }
      }
      _task.steps.push_back(step_obj);
    }
  } catch (const std::exception &e) {
    _error = "Error parsing YAML file: " + std::string(e.what());
    return false;
  }
  return true;
}

TaskParser::~TaskParser() {}

const Task &TaskParser::getTask() const { return _task; }

std::optional<std::string> TaskParser::getError() const { return _error; }

} // namespace remote_agent
