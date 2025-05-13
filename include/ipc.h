#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>


#include <zmqpp/context.hpp>
#include <zmqpp/zmqpp.hpp>

namespace remote_agent {
class IPCContext {
public:
  static IPCContext &getInstance();
  bool hasPublisher(const std::string& endpoint);
  std::shared_ptr<zmqpp::socket> getPublisher(const std::string& endpoint);
  void addPublisher(const std::string& endpoint,
                    std::shared_ptr<zmqpp::socket> socket);
  void addPublisher(const std::string& endpoint);
  void removePublisher(const std::string& endpoint);
  bool hasSubscriber(const std::string& endpoint);
  std::shared_ptr<zmqpp::socket> getSubscriber(const std::string& endpoint);
  void addSubscriber(const std::string& endpoint,
                     std::shared_ptr<zmqpp::socket> socket);
  void addSubscriber(const std::string& endpoint);
  void removeSubscriber(const std::string& endpoint);
  std::shared_mutex& getMutex(const std::string& endpoint);
  bool hasMutex(const std::string& endpoint);
  zmqpp::context& getContext();

private:
  IPCContext();
  std::string getPort(const std::string& endpoint);

  zmqpp::context _context;
  std::mutex _mutex;
  std::unordered_map<std::string, std::shared_mutex> _sharedMutexes;
  std::unordered_map<std::string, std::shared_ptr<zmqpp::socket>> _publishers;
  std::unordered_map<std::string, uint16_t> _publisherCount;
  std::unordered_map<std::string, std::shared_ptr<zmqpp::socket>> _subscribers;
  std::unordered_map<std::string, uint16_t> _subscriberCount;
};

enum class IPCType { Publisher, Subscriber };

class IPC {
public:
  IPC(const std::string& endpoint, const std::string &topic, IPCType type);
  virtual ~IPC() = default;

  void connect();
  void disconnect();

protected:
  std::string _endpoint;
  std::string _topic;
  std::shared_ptr<zmqpp::socket> _socket;

private:
  IPCType _type;
};
} // namespace remote_agent
