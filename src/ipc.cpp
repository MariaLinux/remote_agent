#include "ipc.h"

#include <mutex>
#include <string>
#include <regex>
#include <zmqpp/message.hpp>

namespace remote_agent {

IPCContext &IPCContext::getInstance() {
  static IPCContext instance;
  return instance;
}

IPCContext::IPCContext() {
}

bool IPCContext::hasPublisher(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  return _publishers.find(endpoint) != _publishers.end();
}

std::shared_ptr<zmqpp::socket>
IPCContext::getPublisher(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  return _publishers[endpoint];
}

void IPCContext::addPublisher(const std::string& endpoint,
                              std::shared_ptr<zmqpp::socket> socket) {
  std::lock_guard<std::mutex> lock(_mutex);
  _publishers[endpoint] = socket;
  _publisherCount[endpoint] = 1;
  _sharedMutexes.try_emplace(getPort(endpoint));
}

void IPCContext::removePublisher(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_publisherCount.find(endpoint) != _publisherCount.end() &&
      _publisherCount[endpoint] == 1) {
    _publishers.erase(endpoint);
    _publisherCount.erase(endpoint);
  } else if (_publisherCount.find(endpoint) != _publisherCount.end() &&
             _publisherCount[endpoint] > 1) {
    _publisherCount[endpoint]--;
  }
}

bool IPCContext::hasSubscriber(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  return _subscribers.find(endpoint) != _subscribers.end();
}

std::shared_ptr<zmqpp::socket>
IPCContext::getSubscriber(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  return _subscribers[endpoint];
}

void IPCContext::addSubscriber(const std::string& endpoint,
                               std::shared_ptr<zmqpp::socket> socket) {
  std::lock_guard<std::mutex> lock(_mutex);
  _subscribers[endpoint] = socket;
  _subscriberCount[endpoint] = 1;
}

void IPCContext::addPublisher(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_publisherCount.find(endpoint) != _publisherCount.end())
    _publisherCount[endpoint]++;
}

void IPCContext::addSubscriber(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_subscriberCount.find(endpoint) != _subscriberCount.end())
    _subscriberCount[endpoint]++;
}

void IPCContext::removeSubscriber(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_subscriberCount.find(endpoint) != _subscriberCount.end() &&
      _subscriberCount[endpoint] == 1) {
    _subscribers.erase(endpoint);
    _subscriberCount.erase(endpoint);
  } else if (_subscriberCount.find(endpoint) != _subscriberCount.end() &&
             _subscriberCount[endpoint] > 1) {
    _subscriberCount[endpoint]--;
  }
}

std::shared_mutex& IPCContext::getMutex(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  return _sharedMutexes[getPort(endpoint)];
}

bool IPCContext::hasMutex(const std::string& endpoint) {
  std::lock_guard<std::mutex> lock(_mutex);
  return _sharedMutexes.find(getPort(endpoint)) != _sharedMutexes.end();
}

zmqpp::context& IPCContext::getContext() {
  std::lock_guard<std::mutex> lock(_mutex);
  return std::ref(_context);
}

std::string IPCContext::getPort(const std::string& endpoint) {
  std::regex port_regex("tcp://.*:(\\d+)");
  std::smatch port_match;
  if (std::regex_search(endpoint, port_match, port_regex)) {
    return port_match[1].str();
  }
  return "";
}

IPC::IPC(const std::string& endpoint, const std::string& topic, IPCType type)
    : _endpoint(endpoint), _topic(topic), _type(type) {}

void IPC::connect() {
  if (_type == IPCType::Publisher) {
    if (!IPCContext::getInstance().hasPublisher(_endpoint)) {
      _socket = std::make_shared<zmqpp::socket>(IPCContext::getInstance().getContext(),
                                                zmqpp::socket_type::publish);
      _socket->bind(_endpoint);
      IPCContext::getInstance().addPublisher(_endpoint, _socket);
    } else {
      _socket = IPCContext::getInstance().getPublisher(_endpoint);
      IPCContext::getInstance().addPublisher(_endpoint);
    }
  } else if (_type == IPCType::Subscriber) {
    if (!IPCContext::getInstance().hasSubscriber(_endpoint + _topic)) {
      _socket = std::make_shared<zmqpp::socket>(IPCContext::getInstance().getContext(),
                                                zmqpp::socket_type::subscribe);
      _socket->connect(_endpoint);
      IPCContext::getInstance().addSubscriber(_endpoint + _topic, _socket);
    } else {
      _socket = IPCContext::getInstance().getSubscriber(_endpoint + _topic);
      IPCContext::getInstance().addSubscriber(_endpoint + _topic);
    }
  }
}

void IPC::disconnect() {
  if (_type == IPCType::Publisher) {
    IPCContext::getInstance().removePublisher(_endpoint);
  } else if (_type == IPCType::Subscriber) {
    IPCContext::getInstance().removeSubscriber(_endpoint);
  }
}

} // namespace remote_agent
