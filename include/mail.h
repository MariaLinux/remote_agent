#pragma once

#include <string>
#include <vector>
#include <list>
#include <optional>
#include <fstream>
#include <tuple>
#include <variant>
#include <random>
#include <filesystem>

#include <mailio/message.hpp>
#include <mailio/mime.hpp>
#include <mailio/smtp.hpp>
#include <mailio/imap.hpp>

#include "config.h"

namespace remote_agent::mail {

    struct Contact {
        std::string name;
        std::string mail_address;
    };

    struct MailGroup {
        std::string name;
        std::vector<Contact> address_list;
    };

    struct Recipient {
        Contact from;
        std::vector<Contact> to_list;
        std::vector<Contact> cc_list;
        std::vector<Contact> bcc_list;
        std::vector<MailGroup> group_to_list;
        std::vector<MailGroup> group_cc_list;
        std::vector<MailGroup> group_bcc_list;
    };

    using File = std::tuple<std::istream&, std::string, std::string>;
    using Protocol = std::variant<
                                mailio::smtp*, 
                                mailio::smtps*,
                                mailio::imap*,
                                mailio::imaps*>;

    class Mail {
        public:
            Mail (const AccountConfig &config);

            std::optional<Error> send(const Recipient &recipient, const std::string &subject, const std::string &body);
            std::optional<Error> send(const std::string &subject, const std::string &body);
            std::optional<Error> send(const Recipient &recipient, const std::string &subject, const std::string &body, const std::list<File> &file_list);
            std::optional<Error> send(const std::string &subject, const std::string &body, const std::list<File> &file_list);
            std::pair<uint32_t,std::optional<Error>> count(const std::string& folder);
            std::pair<std::string,std::optional<Error>> getByFilter();            

        private:
            mailio::message prepareMessage(const Recipient& recipient, const std::string& subject, const std::string& body);
            mailio::message prepareMessage(const std::string& subject, const std::string& body);
            std::list<std::tuple<std::istream&, mailio::string_t, mailio::mime::content_type_t>> prepareAttachment(const std::list<File> &file_list);
            Error parseError(const std::string& error);
            std::optional<Error> send(const mailio::message& msg);
            std::pair<std::string,std::optional<Error>> getByFilter(mailio::imap& conn);
            std::optional<Error> authenticate(const Protocol& protocol);
            std::string generateTempFilename();
            std::string getCurrentTimeDirectoryName();

            const AccountConfig &_config;
    };
    
};