#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>


namespace remote_agent {
struct Step {
  std::string name;
  std::vector<std::string> commands;
  std::map<std::string,std::string> environments;
};

struct Task {
  std::string name;
  std::vector<Step> steps;
};

class TaskParser {
public:
  TaskParser();
  TaskParser(const TaskParser& other) = delete;
  TaskParser& operator=(const TaskParser& other) = delete;
  ~TaskParser();
  bool parseYaml(std::string filename);

  const Task& getTask() const;
  std::optional<std::string> getError() const;

private:
  Task _task;
  std::optional<std::string> _error;
};
}