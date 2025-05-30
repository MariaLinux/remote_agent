#include "mail.h"
#include "config.h"

#include <chrono>
#include <iomanip>
#include <mailio/dialog.hpp>
#include <memory>
#include <sstream>
#include <random>
#include <filesystem>
#include <fstream>
#include <syslog.h>

namespace remote_agent::mail {

    Mail::Mail (const AccountConfig &config) : _config{config} {

    }

    std::optional<Error> Mail::send(const Recipient &recipient, const std::string &subject, const std::string &body) {
        const auto& msg = prepareMessage(recipient, subject, body);

        return send(msg);
    }

    std::optional<Error> Mail::send(const Recipient &recipient, const std::string &subject, const std::string &body, const std::list<File> &file_list) {
        auto msg = prepareMessage(recipient, subject, body);
        auto list = prepareAttachment(file_list);
        
        std::list<std::tuple<std::istream&, mailio::string_t, mailio::mime::content_type_t>> attachment_list;
        for (const auto &item : list)
          attachment_list.push_back(std::make_tuple(std::ref(*std::get<0>(item).get()), std::get<1>(item), std::get<2>(item)));
        if (!attachment_list.empty())
            msg.attach(attachment_list);

        return send(msg);        
    }

    std::optional<Error> Mail::send(const std::string &subject, const std::string &body) {
        const auto& msg = prepareMessage(subject, body);

        return send(msg);
    }

    std::optional<Error> Mail::send(const std::string &subject, const std::string &body, const std::list<File> &file_list) {
        auto msg = prepareMessage(subject, body);
        auto list = prepareAttachment(file_list);
        
        std::list<std::tuple<std::istream&, mailio::string_t, mailio::mime::content_type_t>> attachment_list;
        for (const auto &item : list)
          attachment_list.push_back(std::make_tuple(std::ref(*std::get<0>(item).get()), std::get<1>(item), std::get<2>(item)));
        if (!attachment_list.empty())
            msg.attach(attachment_list);

        return send(msg);        
    }

    std::pair<uint32_t,std::optional<Error>> Mail::count(const std::string& folder) {
        std::optional<Error> err;
        uint32_t count = 0;

        try {
            if (_config.imap.security == "ssl") {
                mailio::imaps conn(_config.imap.host , _config.imap.port);
                err = authenticate(&conn);
                mailio::imaps::mailbox_stat_t stat = conn.statistics(folder);
                syslog(LOG_INFO, "Mail count in %s: %ld", folder.c_str(), stat.messages_no);
                count = stat.messages_no;
            } else {
                mailio::imap conn(_config.imap.host , _config.imap.port);
                err = authenticate(&conn);
                mailio::imap::mailbox_stat_t stat = conn.statistics(folder);
                syslog(LOG_INFO, "Mail count in %s: %ld", folder.c_str(), stat.messages_no);
                count = stat.messages_no;
            }
        }
        catch (mailio::imap_error& exc)
        {
            syslog(LOG_ERR, "Mail/count: %s", exc.what());
            err = parseError(exc.what());
        }
        catch (mailio::dialog_error& exc) {
            syslog(LOG_ERR, "Mail/count: %s", exc.what());
            err = parseError(exc.what());
        }
        catch (const std::exception& exc) {
            syslog(LOG_ERR, "Mail/count: %s", exc.what());
            err = parseError(exc.what());
        }
        return std::make_pair(count,err);
    }

    std::pair<std::string,std::optional<Error>> Mail::getByFilter() {
        std::optional<Error> err;
        try {
        if (_config.imap.security == "ssl") {
            mailio::imaps conn(_config.imap.host , _config.imap.port);
            err = authenticate(&conn);
            if (err.has_value())
                return std::make_pair("",err);
            return getByFilter(conn);                
        } else {
            mailio::imap conn(_config.imap.host , _config.imap.port);
            err = authenticate(&conn);
            if (err.has_value())
                return std::make_pair("",err);
            return getByFilter(conn);
        }
    }
     catch (const mailio::dialog_error& exc) {
        syslog(LOG_ERR, "Mail/getByFilter: %s", exc.what());
        err = parseError(exc.what());
        return std::make_pair("",err);
     }
     catch (const std::exception& exc) {
        syslog(LOG_ERR, "Mail/getByFilter: %s", exc.what());
        err = parseError(exc.what());
        return std::make_pair("",err);
     }
    }

    std::pair<std::string,std::optional<Error>> Mail::getByFilter(mailio::imap& conn) {
        std::optional<Error> err;
        std::string work_dir = std::filesystem::path(Config::getInstance().getGlobalConfig().work_dir) / getCurrentTimeDirectoryName();
        try{
            mailio::imap::mailbox_stat_t stat = conn.select(_config.imap_filter.folders);
            std::string folders="(";
            for (auto f : _config.imap_filter.folders){
                folders += f + ",";
            }
            folders.pop_back();
            folders += ")";
            syslog(LOG_INFO, "Mail count in %s: %ld", folders.c_str(), stat.messages_no);
            std::list<unsigned long> messages;
            conn.search(_config.imap_filter.conditions, messages, true);
            if (messages.empty()) {
                syslog(LOG_INFO, "No new mail found");
                return  std::make_pair<std::string,std::optional<Error>>(std::string(),std::make_pair<ErrorCode,std::string>(ErrorCode::NO_NEW_MAIL, "No new mail found"));
            }
            for(unsigned int msg_uid : messages) { 
                mailio::message msg;
                std::string msg_str;
                conn.fetch(msg_uid, msg, true);
                msg.format(msg_str);
                if (!std::filesystem::exists(work_dir))
                    if(std::filesystem::create_directories(work_dir))
                        syslog(LOG_INFO, "Mail directory created: %s", work_dir.c_str());
                std::ofstream fos_text(std::filesystem::path(work_dir) / "mail.txt");
                if (fos_text.is_open()) {
                    fos_text << msg_str;
                    fos_text.close();
                    syslog(LOG_INFO, "Mail content has written into: %s", std::string(work_dir + "mail.txt").c_str());
                }
                for (std::size_t i = 1; i <= msg.attachments_size(); i++) {
                    std::string tmp_file = generateTempFilename();
                    std::ofstream fos(tmp_file, std::ios::binary);
                    mailio::string_t name;
                    msg.attachment(i, fos, name);
                    fos.close();
                    syslog(LOG_INFO, "Mail attachment (%ld) has written to %s", i, tmp_file.c_str());
                    if(std::filesystem::copy_file(tmp_file, std::filesystem::path(work_dir) / name.buffer)) {
                        syslog(LOG_INFO, "Mail attachment renamed into: %s", std::string(work_dir + name.buffer).c_str());
                        std::filesystem::remove(tmp_file);
                    }
                }
            }
        }
        catch (mailio::message_error& exc)
        {
            syslog(LOG_ERR, "Mail/getByFilter: %s", exc.what());
            err = parseError(exc.what());
        }
        catch (mailio::imap_error& exc)
        {
            syslog(LOG_ERR, "Mail/getByFilter: %s", exc.what());
            err = parseError(exc.what());
        }
        catch (mailio::dialog_error& exc) {
            syslog(LOG_ERR, "Mail/getByFilter: %s", exc.what());
            err = parseError(exc.what());
        }
        catch (std::filesystem::filesystem_error& exc) {
            syslog(LOG_ERR, "Mail/getByFilter: %s", exc.what());
            err = parseError(exc.what());
        }
        catch (const std::exception& exc) {
            syslog(LOG_ERR, "Mail/getByFilter: %s", exc.what());
            err = parseError(exc.what());
        }
        return std::make_pair(work_dir, err);
    }

    std::optional<Error> Mail::send(const mailio::message &msg) {
        std::optional<Error> mail_error;
        try {
            std::string res;
            if (_config.smtp.security == "ssl") {
                mailio::smtps conn(_config.smtp.host , _config.smtp.port);
                mail_error = authenticate(&conn);
                res = conn.submit(msg);
                syslog(LOG_INFO, "Mail/send: %s", res.c_str());
            } else {
                mailio::smtp conn(_config.smtp.host , _config.smtp.port);
                mail_error = authenticate(&conn);
                res = conn.submit(msg);
                syslog(LOG_INFO, "Mail/send: %s", res.c_str());
            }
        }
        catch (mailio::smtp_error& exc) {
            syslog(LOG_ERR, "Mail/send: %s", exc.what());
            mail_error = parseError(exc.what());
        }
        catch (mailio::codec_error& exc) {
            syslog(LOG_ERR, "Mail/send: %s", exc.what());
            mail_error = parseError(exc.what());
        }
        catch (mailio::dialog_error& exc) {
            syslog(LOG_ERR, "Mail/send: %s", exc.what());
            mail_error = parseError(exc.what());
        }
        catch (const std::exception& exc) {
            syslog(LOG_ERR, "Mail/send: %s", exc.what());
            mail_error = parseError(exc.what());
        }
        return mail_error;
    }

    mailio::message Mail::prepareMessage(const Recipient &recipient, const std::string &subject, const std::string &body) {
        mailio::message msg;

        msg.subject(subject, mailio::codec::codec_t::UTF8);
        msg.content_transfer_encoding(mailio::mime::content_transfer_encoding_t::QUOTED_PRINTABLE);
        msg.content_type(mailio::message::media_type_t::TEXT, "plain", "utf-8");
        msg.content(body);

        msg.from(mailio::mail_address(mailio::string_t(recipient.from.name, "UTF-8", mailio::codec::codec_t::UTF8), recipient.from.mail_address));
        
        for (const auto &item : recipient.to_list)
            msg.add_recipient(mailio::mail_address(mailio::string_t(item.name, "UTF-8", mailio::codec::codec_t::UTF8), item.mail_address));
        
        for (const auto &item : recipient.cc_list)
            msg.add_cc_recipient(mailio::mail_address(mailio::string_t(item.name, "UTF-8", mailio::codec::codec_t::UTF8), item.mail_address));

        for (const auto &item : recipient.bcc_list)
            msg.add_bcc_recipient(mailio::mail_address(mailio::string_t(item.name, "UTF-8", mailio::codec::codec_t::UTF8), item.mail_address));

        for (const auto &item : recipient.group_to_list) {
            std::vector<mailio::mail_address> mail_list;
            for (const auto &sub_item : item.address_list)
                mail_list.push_back(mailio::mail_address(mailio::string_t(sub_item.name, "UTF-8", mailio::codec::codec_t::UTF8), sub_item.mail_address));
            msg.add_recipient(mailio::mail_group(item.name, mail_list));
        }

        for (const auto &item : recipient.group_cc_list) {
            std::vector<mailio::mail_address> mail_list;
            for (const auto &sub_item : item.address_list)
                mail_list.push_back(mailio::mail_address(mailio::string_t(sub_item.name, "UTF-8", mailio::codec::codec_t::UTF8), sub_item.mail_address));
            msg.add_cc_recipient(mailio::mail_group(item.name, mail_list));
        }

        for (const auto &item : recipient.group_bcc_list) {
            std::vector<mailio::mail_address> mail_list;
            for (const auto &sub_item : item.address_list)
                mail_list.push_back(mailio::mail_address(mailio::string_t(sub_item.name, "UTF-8", mailio::codec::codec_t::UTF8), sub_item.mail_address));
            msg.add_bcc_recipient(mailio::mail_group(item.name, mail_list));
        }
        return std::move(msg);
    }

    mailio::message Mail::prepareMessage(const std::string& subject, const std::string& body) {
        mailio::message msg;

        if (!_config.smtp_details.has_value()) {
            syslog(LOG_ERR, "Mail/prepareMessage: Bad config for sending emails in smtp config");
            return msg;
        }
        msg.subject(subject, mailio::codec::codec_t::UTF8);
        msg.content_transfer_encoding(mailio::mime::content_transfer_encoding_t::QUOTED_PRINTABLE);
        msg.content_type(mailio::message::media_type_t::TEXT, "plain", "utf-8");
        msg.content(body);
        auto config = _config.smtp_details.value();

        msg.from(config.from);
        
        for (const auto &item : config.to.addresses)
            msg.add_recipient(item);
        
        for (const auto &item : config.cc.addresses)
            msg.add_cc_recipient(item);

        for (const auto &item : config.bcc.addresses)
            msg.add_bcc_recipient(item);

        for (const auto &item : config.to.groups) 
            msg.add_recipient(item);

        for (const auto &item : config.cc.groups) 
            msg.add_cc_recipient(item);

        for (const auto &item : config.bcc.groups) 
            msg.add_bcc_recipient(item);

        return std::move(msg);
    }

    std::list<std::tuple<std::shared_ptr<std::istream>, mailio::string_t, mailio::mime::content_type_t>> Mail::prepareAttachment(const std::list<File> &file_list) {
        std::list<std::tuple<std::shared_ptr<std::istream>, mailio::string_t, mailio::mime::content_type_t>> attachment_list;
        for (const auto &file : file_list) {
            mailio::message::content_type_t mime_type;
            std::string lower_mime = boost::to_lower_copy(file.second);
            auto separator_pos = lower_mime.find('/');

            if (separator_pos != std::string::npos) {
                std::string media_type_str = lower_mime.substr(0, separator_pos);
                std::string media_subtype = lower_mime.substr(separator_pos + 1);
                if (media_type_str == "text")
                    mime_type = mailio::message::content_type_t(mailio::message::media_type_t::TEXT, media_subtype);
                else if (media_type_str == "image")
                    mime_type = mailio::message::content_type_t(mailio::message::media_type_t::IMAGE, media_subtype);
                else if (media_type_str == "audio")
                    mime_type = mailio::message::content_type_t(mailio::message::media_type_t::AUDIO, media_subtype);
                else if (media_type_str == "video")
                    mime_type = mailio::message::content_type_t(mailio::message::media_type_t::VIDEO, media_subtype);
                else if (media_type_str == "application")
                    mime_type = mailio::message::content_type_t(mailio::message::media_type_t::APPLICATION, media_subtype);
                else if (media_type_str == "multipart")
                    mime_type = mailio::message::content_type_t(mailio::message::media_type_t::MULTIPART, media_subtype);
                else if (media_type_str == "message")
                    mime_type = mailio::message::content_type_t(mailio::message::media_type_t::MESSAGE, media_subtype);
                else
                    mime_type = mailio::message::content_type_t(mailio::message::media_type_t::TEXT, "plain");
            } else {
                mime_type = mailio::message::content_type_t(mailio::message::media_type_t::TEXT, "plain");
            }
            std::shared_ptr<std::ifstream> ifs = std::make_shared<std::ifstream>(file.first, std::ios::binary);
            attachment_list.push_back(std::make_tuple(
                ifs,
                mailio::string_t(std::filesystem::path(file.first).filename().string(), "ASCII", mailio::codec::codec_t::ASCII),
                mime_type
            ));
        }
        return std::move(attachment_list);
    }

    Error Mail::parseError(const std::string &error) { 
        std::string lower_err = boost::to_lower_copy(error);
        ErrorCode code;
        if (lower_err.find("network") != std::string::npos)
            code = ErrorCode::NETWORK;
        else if (lower_err.find("rejection") != std::string::npos)
            code = ErrorCode::REJECTION;
        else
            code = ErrorCode::UNKNOWN;
        return std::make_pair(code, error);
    }

    std::optional<Error> Mail::authenticate(const Protocol& protocol) {
        std::optional<Error> mail_error;
        std::string res;

        try {
            if (std::holds_alternative<mailio::smtp*>(protocol)) {
                auto conn = std::get<mailio::smtp*>(protocol);
                std::string lower_auth = boost::to_lower_copy(_config.smtp.auth_method);
                mailio::smtp::auth_method_t method = mailio::smtp::auth_method_t::NONE;
                if (lower_auth.find("login") != std::string::npos)
                    method = mailio::smtp::auth_method_t::LOGIN;
                res = conn->authenticate(_config.credentials.username, _config.credentials.password, method);
                syslog(LOG_INFO, "authenticate(smtp): %s", res.c_str());
            } else if (std::holds_alternative<mailio::smtps*>(protocol)) {
                auto conn = std::get<mailio::smtps*>(protocol);
                std::string lower_auth = boost::to_lower_copy(_config.smtp.auth_method);
                mailio::smtps::auth_method_t method = mailio::smtps::auth_method_t::NONE;
                if (lower_auth.find("start_tls") != std::string::npos)
                    method = mailio::smtps::auth_method_t::START_TLS;
                else if (lower_auth.find("login") != std::string::npos)
                    method = mailio::smtps::auth_method_t::LOGIN;
                res = conn->authenticate(_config.credentials.username, _config.credentials.password, method);
                syslog(LOG_INFO, "authenticate(smtps): %s", res.c_str());
            } else if (std::holds_alternative<mailio::imaps*>(protocol)) {
                auto conn = std::get<mailio::imaps*>(protocol);
                std::string lower_auth = boost::to_lower_copy(_config.imap.auth_method);
                mailio::imaps::auth_method_t method = mailio::imaps::auth_method_t::LOGIN;
                if (lower_auth.find("start_tls") != std::string::npos)
                    method = mailio::imaps::auth_method_t::START_TLS;
                res = conn->authenticate(_config.credentials.username, _config.credentials.password, method);
                syslog(LOG_INFO, "authenticate(imaps): %s", res.c_str());
            } else if (std::holds_alternative<mailio::imap*>(protocol)) {
                auto conn = std::get<mailio::imap*>(protocol);
                res = conn->authenticate(_config.credentials.username, _config.credentials.password, mailio::imap::auth_method_t::LOGIN);
                syslog(LOG_INFO, "authenticate(imap): %s", res.c_str());
            }
        }
        catch (mailio::imap_error& exc) {
            syslog(LOG_ERR, "Mail/authenticate: %s", exc.what());
            mail_error = parseError(exc.what());
        }
        catch (mailio::smtp_error& exc) {
            syslog(LOG_ERR, "Mail/authenticate: %s", exc.what());
            mail_error = parseError(exc.what());
        }
        catch (mailio::dialog_error& exc) {
            syslog(LOG_ERR, "Mail/authenticate: %s", exc.what());
            mail_error = parseError(exc.what());
        }
        catch (const std::exception& exc) {
            syslog(LOG_ERR, "Mail/authenticate: %s", exc.what());
            mail_error = parseError(exc.what());
        }
        return mail_error;
    }

    std::string Mail::generateTempFilename() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static const char* hex = "0123456789abcdef";
        
        std::string uuid;
        for (int i = 0; i < 16; ++i) {
            uuid += hex[dis(gen)];
        }
        
        return std::filesystem::temp_directory_path() / ("temp-" + uuid + ".tmp");
    }

    std::string Mail::getCurrentTimeDirectoryName() {
        auto now = std::chrono::system_clock::now();
        auto millisec = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto timer = std::chrono::system_clock::to_time_t(now);
        std::tm bt = *std::localtime(&timer);
        
        std::stringstream ss;
        ss << std::put_time(&bt, "%Y-%m-%d_%H-%M-%S") << "_" << std::setfill('0') << std::setw(3) << millisec.count();
        
        return ss.str();
    }
};