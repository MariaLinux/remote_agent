#pragma once

#include <boost/process/v1/detail/child_decl.hpp>
#include <tuple>
#include <string>

#include <boost/process.hpp>

namespace remote_agent {
  using CommandResult = std::tuple<int, std::string, std::string>;
  enum class Shell {
    SH,
    BASH,
    ZSH
  };
  
  class Runner {
    public:
      Runner();
      CommandResult execute(const std::string& command);
      void setShell(Shell shell);
      
    private:
      CommandResult readStream(boost::process::child& process, boost::process::ipstream& out_stream, boost::process::ipstream& err_stream);
    
      bool _default_shell;
      Shell _shell;
  };
}