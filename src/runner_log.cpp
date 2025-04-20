#include "runner_log.h"
#include <string>

void register_log_file(const std::string &file_name) {
  static std::string static_file_name = "";
  if (static_file_name == file_name)
    return;
  else if (static_file_name != "")
    deregister_log();
  static_file_name = file_name;
  boost::log::add_common_attributes();
  boost::log::add_file_log(
      boost::log::keywords::file_name = file_name,
      boost::log::keywords::format =
          (boost::log::expressions::stream
           << boost::log::expressions::if_(
                  boost::log::expressions::has_attr(is_raw) &&
                  is_raw)[boost::log::expressions::stream
                          << boost::log::expressions::smessage]
                  .else_[boost::log::expressions::stream
                         << "["
                         << boost::log::expressions::format_date_time<
                                boost::posix_time::ptime>(
                                "TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                         << "]"
                         << "[" << boost::log::trivial::severity << "] "
                         << boost::log::expressions::smessage]),
      boost::log::keywords::auto_flush = true);
}

void deregister_log() {
  boost::log::core::get()->flush();
  boost::log::core::get()->remove_all_sinks();
}
