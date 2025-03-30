#include "file_utils.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <sys/syslog.h>
#include <syslog.h>
#include <utility>

#include "config.h"

namespace remote_agent {
  Zip::Zip(): 
  _segment_size{0}, _append{0}, _include_path{0}, _recursive{1} {
    
  }
  
  std::optional<Error> Zip::compress(const std::vector<std::string>& file_list, const std::string& destination_file) {
    if (std::filesystem::exists(destination_file)){
      syslog(LOG_ERR, "Zip/compress: file %s has already exists.", destination_file.c_str());
      return make_pair(ErrorCode::FILE_ALREADY_EXISTS, "file " + destination_file+ " has already exists.");
    }
    // void *writer = mz_zip_writer_create();
    auto cleanup = [](void *writer) {
      mz_zip_writer_delete(&writer);
    };
    std::unique_ptr<void, decltype(cleanup)> writer(mz_zip_writer_create(),cleanup);
    if (!writer.get()) {
      syslog(LOG_ERR, "Zip/compress: memory error in mz_zip_writer_create");
      return std::make_pair(ErrorCode::MEMORY_ERROR, "Memory error: mz_zip_writer_create");
    }
    int32_t res = mz_zip_writer_open_file(writer.get(), destination_file.c_str(), _segment_size, _append);
    if (res != MZ_OK) {
      syslog(LOG_ERR, "Zip/compress: error %d opening archive for writing", res);
      return std::make_pair(ErrorCode::FILE_OPEN_FAILED, "Error "+ std::to_string(res) + " opening archive for writing");
    }
    for (int32_t i = 0; i < file_list.size(); i++) {
      res = mz_zip_writer_add_path(writer.get(), file_list[i].c_str(), NULL,
                                   _include_path, _recursive);
      if (res != MZ_OK) {
        syslog(LOG_ERR, "Error %d adding path to archive %s", res, file_list[i].c_str());
      }
    }
    res = mz_zip_writer_close(writer.get());
    if (res != MZ_OK) {
      syslog(LOG_ERR, "Zip/compress: Error %d closing archive", res);
      return std::make_pair(ErrorCode::FILE_CLOSE_FAILED, "Error "+ std::to_string(res) + " closing archive");
    }
    syslog(LOG_INFO, "Zip/compress: successfully created at %s", destination_file.c_str());
    return std::nullopt;
  }
  
  std::optional<Error> Zip::extract(const std::string& zip_file, const std::string& destination_dir) {
    if (!std::filesystem::exists(destination_dir)){
      if (!std::filesystem::create_directories(destination_dir)) {
        syslog(LOG_ERR, "Zip/extract: directory creation failed %s", destination_dir.c_str());
        return std::make_pair(ErrorCode::FILE_CREATE_FAILED, "directory creation failed" + destination_dir);
      }
    }
    auto cleanup = [](void *reader) {
      mz_zip_reader_delete(&reader);
    };
    // void *reader = mz_zip_reader_create();
    std::unique_ptr<void, decltype(cleanup)> reader(mz_zip_reader_create(),cleanup);
    if (!reader.get()) {
      syslog(LOG_ERR, "Zip/extract: memory error in mz_zip_reader_create");
      return std::make_pair(ErrorCode::MEMORY_ERROR, "Memory error: mz_zip_reader_create");
    }
  
    int32_t res = mz_zip_reader_open_file(reader.get(), zip_file.c_str());
    if (res != MZ_OK) {
      syslog(LOG_ERR, "Zip/extract: error %d opening archive %s for reading", res, zip_file.c_str());
      return std::make_pair(ErrorCode::FILE_OPEN_FAILED, "Error "+ std::to_string(res) + " opening archive " + zip_file + " for reading");
    }
  
    res = mz_zip_reader_save_all(reader.get(), destination_dir.c_str());
    if (res != MZ_OK) {
      return std::make_pair(ErrorCode::UNARCHIVE_FAILED, "Error "+ std::to_string(res) + " extracting archive " + zip_file);
    }
    syslog(LOG_INFO, "Zip/extract: successfully extracted to %s", destination_dir.c_str());
    return std::nullopt;
  }
  
  void Zip::setSegmentSize(uint64_t segment_size) {
    _segment_size = segment_size;
  }
  
  void Zip::enableAppend() {
    _append = 1;
  }
  
  void Zip::disableAppend() {
    _append = 0;
  }
  
  void Zip::enableIncludePath() {
    _include_path = 1;
  }
  
  void Zip::disableIncludePath() {
    _include_path = 0;
  }
  
  void Zip::enableRecursive() {
    _recursive = 1;
  }
  
  void Zip::disableRecursive() {
    _recursive = 0;
  }
};