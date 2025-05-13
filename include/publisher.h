#pragma once

#include <mutex>
#include <string>
#include <zmqpp/socket.hpp>
#include <zmqpp/zmqpp.hpp>

#include "ipc.h"

namespace remote_agent {

template <typename Msg> class Publisher : public IPC {
public:
  Publisher(const std::string& endpoint, const std::string& topic): IPC(endpoint,topic, IPCType::Publisher) {}

   void publish(const Msg& message) {
    if (!IPCContext::getInstance().hasMutex(_endpoint)) {
      zmqpp::message msg;
      msg << _topic << message;
      _socket->send(msg);
      return;
    }
    std::unique_lock lock(IPCContext::getInstance().getMutex(_endpoint));
    zmqpp::message msg;
    msg << _topic << message;
    _socket->send(msg);
  }
};
} // namespace remote_agent
