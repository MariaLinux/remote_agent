#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <iostream>

#include <zmqpp/context.hpp>
#include <zmqpp/poller.hpp>
#include <zmqpp/zmqpp.hpp>

#include "ipc.h"

namespace remote_agent {

template <typename Msg> class Subscriber : public IPC {
public:
  Subscriber(const std::string &endpoint, const std::string &topic): IPC(endpoint,topic, IPCType::Subscriber), _running(false) {}
  
  ~Subscriber() {
    stop();
  }

  void subscribe(std::function<void(const Msg&)> callback){
    _socket->subscribe(_topic);
    _running = true;
    _callback = callback;
    
    _thread = std::thread([this](){
      zmqpp::poller poller;
      poller.add(*_socket.get(), zmqpp::poller::poll_in);
      while(_running){
        if (poller.poll(100)) {
          if (poller.has_input(*_socket.get())) {
            std::cout << "Received message " << std::endl;
            if (IPCContext::getInstance().hasMutex(_endpoint)){
              std::shared_lock lock(IPCContext::getInstance().getMutex(_endpoint));
              zmqpp::message message;
              _socket->receive(message);
              std::string topic;
              Msg msg;
              message >> topic >> msg;
              if (topic == _topic && _callback)
                _callback(msg);
            } else {
              zmqpp::message message;
              _socket->receive(message);
              std::string topic;
              Msg msg;
              message >> topic >> msg;
              if (topic == _topic && _callback) 
                _callback(msg);
            }
          }
        }
      }
    });
    
  }
  
  void stop() {
     _running = false;
     if (_thread.joinable()) {
       _thread.join();
     }
  }
  
  private:
  zmqpp::context _context;
  std::atomic_bool _running;
  std::thread _thread;
  std::function<void(const Msg&)> _callback;
};
} // namespace remote_agent
