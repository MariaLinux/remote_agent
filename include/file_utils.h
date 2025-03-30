#pragma once

#include <cstdint>
#include <string>
#include <vector>


#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_buf.h"
#include "mz_strm_split.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

#include "config.h"

namespace remote_agent {
  class Zip {
    public:
      Zip();
      std::optional<Error> compress(const std::vector<std::string>& file_list, const std::string& destination_file);
      std::optional<Error> extract(const std::string& zip_file, const std::string& destination_dir);
      void setSegmentSize(uint64_t segment_size);
      void enableAppend();
      void disableAppend();
      void enableIncludePath();
      void disableIncludePath();
      void enableRecursive();
      void disableRecursive();
    
    private:
      
    
      uint64_t _segment_size;
      uint8_t _append;
      uint8_t _include_path;
      uint8_t _recursive;
  };
};