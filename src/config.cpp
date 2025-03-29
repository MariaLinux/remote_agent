#include "config.h"

#include <filesystem>
#include <string>
#include <sys/syslog.h>
#include <syslog.h>

namespace remote_agent {

Config Config::getInstance(std::string config_path) {
  static Config instance;
  if (!config_path.empty()) {
    instance.loadConfig(config_path);
  }
  return instance;
}

Config::Config() {}

const GlobalConfig &Config::getGlobalConfig() const { return _global_config; }

const std::vector<AccountConfig> &Config::getAccounts() const {
  return _accounts;
}

std::optional<AccountConfig>
Config::getAccountByName(const std::string &name) const {
  for (const auto &account : _accounts) {
    if (account.name == name) {
      return account;
    }
  }
  return std::nullopt;
}

void Config::loadConfig(const std::string &config_path) {
  try {
    YAML::Node config = YAML::LoadFile(config_path);

    // Parse global configuration
    if (config["global"]) {
      auto global = config["global"];
      _global_config.default_timeout = global["default_timeout"].as<int>(30);
      _global_config.log_level = global["log_level"].as<std::string>("info");
      _global_config.work_dir = global["work_dir"].as<std::string>("/tmp");
    }
    loadDotEnvFile();

    // Parse accounts
    if (config["accounts"]) {
      for (const auto &account_node : config["accounts"]) {
        AccountConfig account;
        account.name = account_node["name"].as<std::string>();

        // Parse SMTP configuration
        if (account_node["protocol"]["smtp"]) {
          auto smtp = account_node["protocol"]["smtp"];
          account.smtp.host = smtp["host"].as<std::string>();
          account.smtp.port = smtp["port"].as<int>();
          account.smtp.security = smtp["security"].as<std::string>("plain");
          account.smtp.auth_method =
              smtp["auth_method"].as<std::string>("login");
          account.smtp_details = parseSmtpConfig(smtp);
        }

        // Parse IMAP configuration
        if (account_node["protocol"]["imap"]) {
          auto imap = account_node["protocol"]["imap"];
          account.imap.host = imap["host"].as<std::string>();
          account.imap.port = imap["port"].as<int>();
          account.imap.security = imap["security"].as<std::string>("plain");
          account.imap.auth_method =
              imap["auth_method"].as<std::string>("login");
          auto filter = parseImapFilter(imap);
          if (filter.has_value())
            account.imap_filter = filter.value();
        }

        // Parse credentials
        if (account_node["credentials"]) {
          auto creds = account_node["credentials"];
          account.credentials.username = creds["username"].as<std::string>();
          account.credentials.password = creds["password"].as<std::string>();

          // Handle environment variables in password
          if (account.credentials.password.substr(0, 2) == "${" &&
              account.credentials.password.back() == '}') {
            std::string env_var = account.credentials.password.substr(
                2, account.credentials.password.length() - 3);
            const char *env_val = std::getenv(env_var.c_str());
            if (env_val) {
              account.credentials.password = env_val;
            } else {
              syslog(LOG_WARNING,
                     "Config/loadConfig: Environment variable %s not found for "
                     "account %s",
                     env_var.c_str(), account.name.c_str());
            }
          }
        }

        account.default_folder =
            account_node["default_folder"].as<std::string>("inbox");

        _accounts.push_back(account);
      }
    }
    syslog(LOG_INFO, "Config/loadConfig: config loaded successfully");
  } catch (const YAML::Exception &e) {
    syslog(LOG_ERR, "Config/loadConfig: YAML parsing error %s", e.what());
  }
}

void Config::loadDotEnvFile() {
  std::string envPath;
  if (std::filesystem::exists(std::filesystem::path(_global_config.work_dir) /
                              ".env"))
    envPath = std::filesystem::path(_global_config.work_dir) / ".env";
  else if (std::filesystem::exists(
               std::filesystem::path(std::filesystem::current_path()) / ".env"))
    envPath = std::filesystem::path(std::filesystem::current_path()) / ".env";
  else
    return;
  std::ifstream envFile(envPath);
  if (!envFile.is_open()) {
    syslog(LOG_WARNING, "Couldn't read env file: %s", envPath.c_str());
    return;
  }
  std::string line;
  while (std::getline(envFile, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    size_t delimiterPos = line.find('=');
    if (delimiterPos == std::string::npos) {
      syslog(LOG_WARNING, "Malformed line in .env file: %s", line.c_str());
      continue;
    }
    std::string key = trim(line.substr(0, delimiterPos));
    std::string value = trim(line.substr(delimiterPos + 1));
    if (setenv(key.c_str(), value.c_str(), 1) != 0) {
      syslog(LOG_ERR, "Could not set environment variable: %s", key.c_str());
    }
  }
  envFile.close();
}

std::string Config::trim(const std::string &str) {
  size_t start = str.find_first_not_of(" \t\r\n");
  size_t end = str.find_last_not_of(" \t\r\n");
  return (start == std::string::npos || end == std::string::npos)
             ? ""
             : str.substr(start, end - start + 1);
}

std::optional<ImapFilterConfig>
Config::parseImapFilter(const YAML::Node &imap_node) {
  if (!imap_node["filter"]) {
    return std::nullopt;
  }

  const auto &filter_node = imap_node["filter"];
  ImapFilterConfig filter;

  filter.name = filter_node["name"].as<std::string>();

  // Parse folders array
  if (filter_node["folders"] && filter_node["folders"].IsSequence()) {
    filter.folders = filter_node["folders"].as<std::list<std::string>>();
  } else {
    // If no folders specified, use inbox as default
    filter.folders.push_back("inbox");
  }

  // Parse conditions
  if (filter_node["conditions"] && filter_node["conditions"].IsSequence()) {
    for (const auto &condition_node : filter_node["conditions"]) {
      filter.conditions.push_back(parseCondition(condition_node));
    }
  }

  return filter;
}

mailio::imap::search_condition_t
Config::parseCondition(const YAML::Node &condition_node) {
  std::string type = condition_node["type"].as<std::string>();
  mailio::imap::search_condition_t::key_type key;

  if (type == "ALL")
    key = mailio::imap::search_condition_t::ALL;
  else if (type == "SID_LIST")
    key = mailio::imap::search_condition_t::SID_LIST;
  else if (type == "UID_LIST")
    key = mailio::imap::search_condition_t::UID_LIST;
  else if (type == "SUBJECT")
    key = mailio::imap::search_condition_t::SUBJECT;
  else if (type == "BODY")
    key = mailio::imap::search_condition_t::BODY;
  else if (type == "FROM")
    key = mailio::imap::search_condition_t::FROM;
  else if (type == "TO")
    key = mailio::imap::search_condition_t::TO;
  else if (type == "BEFORE_DATE")
    key = mailio::imap::search_condition_t::BEFORE_DATE;
  else if (type == "ON_DATE")
    key = mailio::imap::search_condition_t::ON_DATE;
  else if (type == "SINCE_DATE")
    key = mailio::imap::search_condition_t::SINCE_DATE;
  else if (type == "NEW")
    key = mailio::imap::search_condition_t::NEW;
  else if (type == "RECENT")
    key = mailio::imap::search_condition_t::RECENT;
  else if (type == "SEEN")
    key = mailio::imap::search_condition_t::SEEN;
  else if (type == "UNSEEN")
    key = mailio::imap::search_condition_t::UNSEEN;
  else
    syslog(LOG_ERR, "Unknown search condition type: %s", type.c_str());

  // Create appropriate value based on the condition type
  mailio::imap::search_condition_t::value_type value;

  if (key == mailio::imap::search_condition_t::ALL) {
    // ALL doesn't need a value
    value = std::monostate{};
  } else if (key == mailio::imap::search_condition_t::SID_LIST ||
             key == mailio::imap::search_condition_t::UID_LIST) {
    // Parse ID list
    if (condition_node["value"]) {
      std::list<mailio::imap::messages_range_t> ranges;
      for (const auto &range_node : condition_node["value"]) {
        if (range_node["range"]) {
          auto range_list =
              range_node["range"].as<std::vector<unsigned long>>();
          if (range_list.size() == 1) {
            // Single ID
            ranges.push_back(
                std::make_pair(range_list[0], std::optional<unsigned long>{}));
          } else if (range_list.size() == 2) {
            // Range of IDs
            ranges.push_back(std::make_pair(range_list[0], range_list[1]));
          }
        }
      }
      value = ranges;
    }
  } else if (key == mailio::imap::search_condition_t::BEFORE_DATE ||
             key == mailio::imap::search_condition_t::ON_DATE ||
             key == mailio::imap::search_condition_t::SINCE_DATE) {
    // Parse date
    std::string date_str = condition_node["value"].as<std::string>();
    value = boost::gregorian::from_string(date_str);
  } else if (condition_node["value"]) {
    // String conditions (SUBJECT, BODY, FROM, TO)
    value = condition_node["value"].as<std::string>();
  } else if (key == mailio::imap::search_condition_t::NEW ||
             key == mailio::imap::search_condition_t::RECENT ||
             key == mailio::imap::search_condition_t::SEEN ||
             key == mailio::imap::search_condition_t::UNSEEN) {
    // Flag conditions don't need a value
    value = std::monostate{};
  }

  return mailio::imap::search_condition_t(key, value);
}

std::optional<SmtpConfig> Config::parseSmtpConfig(const YAML::Node &smtp_node) {
  SmtpConfig smtp_config;
  bool filled;
  if (smtp_node["from"]) {
    smtp_config.from = mailio::mail_address(
        mailio::string_t(smtp_node["from"]["name"].as<std::string>(), "UTF-8",
                         mailio::codec::codec_t::UTF8),
        smtp_node["from"]["address"].as<std::string>());
    filled = true;
  }
  if (smtp_node["to"]) {
    smtp_config.to = parseMailboxes(smtp_node["to"]);
    filled = true;
  }
  if (smtp_node["cc"])
    smtp_config.cc = parseMailboxes(smtp_node["cc"]);
  if (smtp_node["bcc"])
    smtp_config.bcc = parseMailboxes(smtp_node["bcc"]);
  if (filled)
    return smtp_config;
  return std::nullopt;
}

mailio::mailboxes Config::parseMailboxes(const YAML::Node &mail_node) {
  mailio::mailboxes mboxes;
  if (!mail_node.IsSequence())
    return mboxes;
  for (const auto &item_node : mail_node) {
    if (!item_node.IsMap())
      continue;
    if (item_node["address"]) {
      mailio::mail_address addr;
      addr = mailio::mail_address(
          mailio::string_t(item_node["name"].as<std::string>(), "UTF-8",
                           mailio::codec::codec_t::UTF8),
          item_node["address"].as<std::string>());
      mboxes.addresses.push_back(std::move(addr));
    } else if (item_node["group"] && item_node["list"]) {
      mailio::mail_group group;
      group.name = item_node["group"].as<std::string>();
      if (item_node["list"].IsSequence()) {
        for (const auto &member_node : item_node["list"]) {
          mailio::mail_address member;
          member = mailio::mail_address(
              mailio::string_t(member_node["name"].as<std::string>(), "UTF-8",
                               mailio::codec::codec_t::UTF8),
              member_node["address"].as<std::string>());
          group.members.push_back(std::move(member));
        }
      }
      if (!group.members.empty())
        mboxes.groups.push_back(std::move(group));
    }
  }
  return mboxes;
}
}; // namespace remote_agent
