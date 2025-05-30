#pragma once

#include <string>
#include <vector>
#include <list>
#include <optional>
#include <utility>

#include <yaml-cpp/yaml.h>
#include <mailio/imap.hpp>
#include <mailio/mailboxes.hpp>

namespace remote_agent {

struct GlobalConfig {
    int default_timeout;
    std::string log_level;
    std::string work_dir;
    std::string zeromq_endpoint;
    int check_mail_interval_ms;
};

struct ProtocolConfig {
    std::string host;
    int port;
    std::string security;
    std::string auth_method;
};

struct ImapFilterConfig {
    std::string name;
    std::list<std::string> folders;  // Array of folders to search in
    bool read_only;
    std::list<mailio::imap::search_condition_t> conditions;
};

struct SmtpConfig {
    mailio::mail_address from;
    mailio::mailboxes to; 
    mailio::mailboxes cc;  
    mailio::mailboxes bcc; 
};

struct AccountConfig {
    std::string name;

    ProtocolConfig smtp;
    ProtocolConfig imap;

    ImapFilterConfig imap_filter;
    std::optional<SmtpConfig> smtp_details;
    
    struct Credentials {
        std::string username;
        std::string password;
    } credentials;
    
    std::string default_folder;
};

enum class ErrorCode {
    NETWORK,
    REJECTION,
    BAD_CONFIG,
    MEMORY_ERROR,
    FILE_ALREADY_EXISTS,
    FILE_OPEN_FAILED,
    FILE_CREATE_FAILED,
    FILE_CLOSE_FAILED,
    UNARCHIVE_FAILED,
    NO_NEW_MAIL,
    UNKNOWN
};

using Error = std::pair<ErrorCode, std::string>;

class Config {
    public:
        static Config& getInstance(std::string config_path = "");
        
        const GlobalConfig& getGlobalConfig() const;
        const std::vector<AccountConfig>& getAccounts() const;
        std::optional<AccountConfig> getAccountByName(const std::string& name) const;
        
    private:
        Config();
        
        void loadConfig(const std::string& config_path);
        void loadDotEnvFile();
        std::string trim(const std::string& str);
        mailio::imap::search_condition_t parseCondition(const YAML::Node& condition_node);
        std::optional<ImapFilterConfig> parseImapFilter(const YAML::Node& imap_node);
        std::optional<SmtpConfig> parseSmtpConfig(const YAML::Node& smtp_node);
        mailio::mailboxes parseMailboxes(const YAML::Node& mail_node);
        
        GlobalConfig _global_config;
        std::vector<AccountConfig> _accounts;
    };
};