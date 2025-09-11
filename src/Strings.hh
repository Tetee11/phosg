#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Encoding.hh"
#include "Filesystem.hh"
#include "Platform.hh"

#ifdef PHOSG_WINDOWS
// Apparently Windows doesn't have iovec; we define it outside of the phosg
// namespace so code that uses it won't have to special-case Windows or use
// `using iovec = phosg::iovec'.
struct iovec {
  void* iov_base;
  size_t iov_len;
};
#endif

namespace phosg {

template <typename... ArgTs>
void fwrite_fmt(FILE* f, std::format_string<ArgTs...> fmt, ArgTs&&... args) {
  fwritex(f, std::format(fmt, std::forward<ArgTs>(args)...));
}

std::unique_ptr<void, void (*)(void*)> malloc_unique(size_t size);

std::string toupper(const std::string& s);
std::string tolower(const std::string& s);

std::string str_replace_all(const std::string& s, const char* target, const char* replacement);

template <typename StrT>
void strip_trailing_zeroes(StrT& s) {
  size_t index = s.find_last_not_of('\0');
  if (index != StrT::npos) {
    s.resize(index + 1);
  } else if (!s.empty() && s[0] == '\0') {
    s.resize(0); // String is entirely zeroes
  }
}

template <typename StrT>
void strip_trailing_whitespace(StrT& s) {
  size_t index = s.find_last_not_of(" \t\r\n");
  if (index != StrT::npos) {
    s.resize(index + 1);
  } else if (!s.empty() &&
      ((s[0] == ' ') || (s[0] == '\t') || (s[0] == '\r') || (s[0] == '\n'))) {
    s.resize(0); // String is entirely whitespace
  }
}

template <typename StrT>
void strip_leading_whitespace(StrT& s) {
  size_t index = s.find_first_not_of(" \t\r\n");
  if (index != StrT::npos) {
    s = s.substr(index);
  } else if (!s.empty() &&
      ((s[0] == ' ') || (s[0] == '\t') || (s[0] == '\r') || (s[0] == '\n'))) {
    s.resize(0); // String is entirely whitespace
  }
}

template <typename StrT>
void strip_whitespace(StrT& s) {
  size_t start_index = s.find_first_not_of(" \t\r\n");
  size_t end_index = s.find_last_not_of(" \t\r\n");
  if (start_index != StrT::npos && end_index != StrT::npos) {
    if (start_index) {
      s = s.substr(start_index, end_index - start_index + 1);
    } else {
      s.resize(end_index + 1);
    }
  } else if (!s.empty() &&
      ((s[0] == ' ') || (s[0] == '\t') || (s[0] == '\r') || (s[0] == '\n'))) {
    s.resize(0); // String is entirely whitespace
  }
}

template <typename StrT>
void strip_multiline_comments(StrT& s, bool allow_unterminated = false) {
  bool is_in_comment = false;
  size_t write_offset = 0;
  for (size_t z = 0; z < s.size();) {
    if (!is_in_comment) {
      if ((s[z] == '/') && (z + 1 < s.size()) && (s[z + 1] == '*')) {
        is_in_comment = true;
        z += 2;
      } else {
        s[write_offset++] = s[z++];
      }
    } else {
      if ((s[z] == '*') && (z + 1 < s.size()) && (s[z + 1] == '/')) {
        is_in_comment = false;
        z += 2;
      } else {
        if (s[z++] == '\n') {
          s[write_offset++] = '\n';
        }
      }
    }
  }
  s.resize(write_offset);

  if (!allow_unterminated && is_in_comment) {
    throw std::runtime_error("unterminated multiline comment");
  }
}

std::string escape_quotes(const std::string& s);
std::string escape_controls(const std::string& s, bool escape_non_ascii);
std::string escape_url(const std::string& s, bool escape_slash = false);

inline std::string escape_controls_ascii(const std::string& s) {
  return escape_controls(s, true);
}
inline std::string escape_controls_utf8(const std::string& s) {
  return escape_controls(s, false);
}

uint8_t value_for_hex_char(char x);

// windows.h apparently #defines ERROR, hence the prefixes here :|
enum class LogLevel : int {
  L_USE_DEFAULT = -1,
  L_DEBUG = 0,
  L_INFO = 1,
  L_WARNING = 2,
  L_ERROR = 3,
  L_DISABLED = 4,
};

template <>
LogLevel enum_for_name<LogLevel>(const char* name);
template <>
const char* name_for_enum<LogLevel>(LogLevel level);

LogLevel log_level();
void set_log_level(LogLevel new_level);

inline bool should_log(LogLevel incoming_level, LogLevel min_level) {
  return (static_cast<int>(incoming_level) >= static_cast<int>(min_level));
}

inline bool should_log(LogLevel incoming_level) {
  return should_log(incoming_level, log_level());
}

void print_log_prefix(FILE* stream, LogLevel level);

template <LogLevel Level, typename... ArgTs>
bool log_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) {
  if (!should_log(Level, log_level())) {
    return false;
  }
  print_log_prefix(stderr, Level);
  fwrite_fmt(stderr, fmt, std::forward<ArgTs>(args)...);
  fputc('\n', stderr);
  return true;
}

template <typename... ArgTs>
bool log_debug_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) {
  return log_f<LogLevel::L_DEBUG>(std::forward<std::format_string<ArgTs...>>(fmt), std::forward<ArgTs>(args)...);
}
template <typename... ArgTs>
bool log_info_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) {
  return log_f<LogLevel::L_INFO>(std::forward<std::format_string<ArgTs...>>(fmt), std::forward<ArgTs>(args)...);
}
template <typename... ArgTs>
bool log_warning_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) {
  return log_f<LogLevel::L_WARNING>(std::forward<std::format_string<ArgTs...>>(fmt), std::forward<ArgTs>(args)...);
}
template <typename... ArgTs>
bool log_error_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) {
  return log_f<LogLevel::L_ERROR>(std::forward<std::format_string<ArgTs...>>(fmt), std::forward<ArgTs>(args)...);
}

struct PrefixedLogger {
  std::string prefix;
  LogLevel min_level;

  explicit PrefixedLogger(const std::string& prefix, LogLevel min_level = LogLevel::L_USE_DEFAULT);

  PrefixedLogger sub(const std::string& prefix, LogLevel min_level = LogLevel::L_USE_DEFAULT) const;

  inline LogLevel effective_level() const {
    return this->min_level == LogLevel::L_USE_DEFAULT ? log_level() : this->min_level;
  }

  inline bool should_log(LogLevel incoming_level) const {
    return (static_cast<int>(incoming_level) >= static_cast<int>(this->effective_level()));
  }

  template <LogLevel Level, typename... ArgTs>
  bool log_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) const {
    if (!this->should_log(Level)) {
      return false;
    }
    print_log_prefix(stderr, Level);
    fwritex(stderr, this->prefix);
    fwrite_fmt(stderr, fmt, std::forward<ArgTs>(args)...);
    fputc('\n', stderr);
    return true;
  }

  template <typename... ArgTs>
  bool debug_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) const {
    return this->log_f<LogLevel::L_DEBUG>(std::forward<std::format_string<ArgTs...>>(fmt), std::forward<ArgTs>(args)...);
  }
  template <typename... ArgTs>
  bool info_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) const {
    return this->log_f<LogLevel::L_INFO>(std::forward<std::format_string<ArgTs...>>(fmt), std::forward<ArgTs>(args)...);
  }
  template <typename... ArgTs>
  bool warning_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) const {
    return this->log_f<LogLevel::L_WARNING>(std::forward<std::format_string<ArgTs...>>(fmt), std::forward<ArgTs>(args)...);
  }
  template <typename... ArgTs>
  bool error_f(std::format_string<ArgTs...> fmt, ArgTs&&... args) const {
    return this->log_f<LogLevel::L_ERROR>(std::forward<std::format_string<ArgTs...>>(fmt), std::forward<ArgTs>(args)...);
  }
};

std::vector<std::string> split(const std::string& s, char delim, size_t max_splits = 0);
std::vector<std::wstring> split(const std::wstring& s, wchar_t delim, size_t max_splits = 0);
std::vector<std::string> split_context(const std::string& s, char delim, size_t max_splits = 0);
std::vector<std::string> split_args(const std::string& s);

template <typename ItemContainerT, typename DelimiterT>
std::string join(const ItemContainerT& items, DelimiterT& delim) {
  std::string ret;
  for (const auto& item : items) {
    if (!ret.empty()) {
      ret += delim;
    }
    ret += item;
  }
  return ret;
}

template <typename ItemContainerT>
std::string join(const ItemContainerT& items) {
  std::string ret;
  for (const auto& item : items) {
    ret += item;
  }
  return ret;
}

size_t skip_whitespace(const std::string& s, size_t offset);
size_t skip_whitespace(const char* s, size_t offset);
size_t skip_non_whitespace(const std::string& s, size_t offset);
size_t skip_non_whitespace(const char* s, size_t offset);
size_t skip_word(const std::string& s, size_t offset);
size_t skip_word(const char* s, size_t offset);

std::string string_for_error(int error);

enum class TerminalFormat {
  END = -1,
  NORMAL = 0,
  BOLD = 1,
  UNDERLINE = 4,
  BLINK = 5,
  INVERSE = 7,
  FG_BLACK = 30,
  FG_RED = 31,
  FG_GREEN = 32,
  FG_YELLOW = 33,
  FG_BLUE = 34,
  FG_MAGENTA = 35,
  FG_CYAN = 36,
  FG_GRAY = 37,
  FG_WHITE = 38,
  BG_BLACK = 40,
  BG_RED = 41,
  BG_GREEN = 42,
  BG_YELLOW = 43,
  BG_BLUE = 44,
  BG_MAGENTA = 45,
  BG_CYAN = 46,
  BG_GRAY = 47,
  BG_WHITE = 48,
};

std::string vformat_color_escape(TerminalFormat color, va_list va);
std::string format_color_escape(TerminalFormat color, ...);
void print_color_escape(FILE* stream, TerminalFormat color, ...);

void print_indent(FILE* stream, int indent_level);

enum PrintDataFlags {
  USE_COLOR = 0x0001, // Force color output (for diffs and non-ASCII)
  PRINT_ASCII = 0x0002, // Print ASCII view on the right
  COLLAPSE_ZERO_LINES = 0x0020, // Skip lines of all zeroes
  SKIP_SEPARATOR = 0x0040, // Instead of " | ", print just " "
  DISABLE_COLOR = 0x0080, // Never use color output
  OFFSET_8_BITS = 0x0100, // Always use 2 hex digits in offset column
  OFFSET_16_BITS = 0x0200, // Always use 4 hex digits in offset column
  OFFSET_32_BITS = 0x0400, // Always use 8 hex digits in offset column
  OFFSET_64_BITS = 0x0800, // Always use 16 hex digits in offset column
};

enum FormatDataFlags {
  SKIP_STRINGS = 0x0001,
  HEX_ONLY = 0x0001,
};

void format_data(
    std::function<void(const void*, size_t)> write_data,
    const struct iovec* iovs,
    size_t num_iovs,
    uint64_t start_address,
    const struct iovec* prev_iovs,
    size_t num_prev_iovs,
    uint64_t flags);

void print_data(
    FILE* stream,
    const struct iovec* iovs,
    size_t num_iovs,
    uint64_t start_address = 0,
    const struct iovec* prev_iovs = nullptr,
    size_t num_prev_iovs = 0,
    uint64_t flags = PrintDataFlags::PRINT_ASCII);
void print_data(
    FILE* stream,
    const std::vector<struct iovec>& iovs,
    uint64_t start_address = 0,
    const std::vector<struct iovec>* prev_iovs = nullptr,
    uint64_t flags = PrintDataFlags::PRINT_ASCII);
void print_data(
    FILE* stream,
    const void* _data,
    uint64_t size,
    uint64_t address = 0,
    const void* _prev = nullptr,
    uint64_t flags = PrintDataFlags::PRINT_ASCII);
void print_data(
    FILE* stream,
    const std::string& data,
    uint64_t address = 0,
    const void* prev = nullptr,
    uint64_t flags = PrintDataFlags::PRINT_ASCII);

std::string format_data(
    const struct iovec* iovs,
    size_t num_iovs,
    uint64_t start_address = 0,
    const struct iovec* prev_iovs = nullptr,
    size_t num_prev_iovs = 0,
    uint64_t flags = PrintDataFlags::PRINT_ASCII);
std::string format_data(
    const std::vector<struct iovec>& iovs,
    uint64_t start_address,
    const std::vector<struct iovec>* prev_iovs = nullptr,
    uint64_t flags = PrintDataFlags::PRINT_ASCII);
std::string format_data(
    const void* data,
    uint64_t size,
    uint64_t start_address = 0,
    const void* prev = nullptr,
    uint64_t flags = PrintDataFlags::PRINT_ASCII);
std::string format_data(
    const std::string& data,
    uint64_t address = 0,
    const void* prev = nullptr,
    uint64_t flags = PrintDataFlags::PRINT_ASCII);

enum ParseDataFlags {
  ALLOW_FILES = 1,
};

std::string parse_data_string(const std::string& s, std::string* mask = nullptr, uint64_t flags = 0);
std::string format_data_string(const std::string& data, const std::string* mask = nullptr, uint64_t flags = 0);
std::string format_data_string(const void* data, size_t size, const void* mask = nullptr, uint64_t flags = 0);

std::string format_size(size_t size, bool include_bytes = false);
size_t parse_size(const char* str);

class BitReader {
public:
  BitReader();
  explicit BitReader(std::shared_ptr<std::string> data, size_t offset = 0);
  BitReader(const void* data, size_t size, size_t offset = 0);
  BitReader(const std::string& data, size_t offset = 0);
  virtual ~BitReader() = default;

  size_t where() const;
  size_t size() const;
  size_t remaining() const;
  void truncate(size_t new_size);
  void go(size_t offset);
  void skip(size_t bits);
  bool eof() const;

  uint64_t pread(size_t offset, uint8_t size = 1);
  uint64_t read(uint8_t size = 1, bool advance = true);

private:
  std::shared_ptr<std::string> owned_data;
  const uint8_t* data;
  size_t length;
  size_t offset;
};

// This class exists because apparently vector<bool> isn't required to store its
// elements continguously, and in many reverse-engineering situations we
// definitely want the bits to all be contiguous.
class BitWriter {
public:
  BitWriter();
  ~BitWriter() = default;

  size_t size() const;
  void reset();

  void truncate(size_t bits);

  void write(bool v);

  inline const std::string& str() {
    return this->data;
  }

private:
  std::string data;
  uint8_t last_byte_unset_bits;
};

class StringReader {
public:
  StringReader();
  explicit StringReader(std::shared_ptr<std::string> data, size_t offset = 0);
  StringReader(const void* data, size_t size, size_t offset = 0);
  StringReader(const std::string& data, size_t offset = 0);
  virtual ~StringReader() = default;

  size_t where() const;
  size_t size() const;
  size_t remaining() const;
  void truncate(size_t new_size);
  void go(size_t offset);
  void skip(size_t bytes);
  bool skip_if(const void* data, size_t size);
  bool eof() const;
  std::string all() const;

  StringReader sub(size_t offset) const;
  StringReader sub(size_t offset, size_t size) const;
  StringReader subx(size_t offset) const;
  StringReader subx(size_t offset, size_t size) const;
  BitReader sub_bits(size_t offset) const;
  BitReader sub_bits(size_t offset, size_t size) const;
  BitReader subx_bits(size_t offset) const;
  BitReader subx_bits(size_t offset, size_t size) const;

  const char* peek(size_t size);

  std::string read(size_t size, bool advance = true);
  std::string readx(size_t size, bool advance = true);
  size_t read(void* data, size_t size, bool advance = true);
  void readx(void* data, size_t size, bool advance = true);
  std::string pread(size_t offset, size_t size) const;
  std::string preadx(size_t offset, size_t size) const;
  size_t pread(size_t offset, void* data, size_t size) const;
  void preadx(size_t offset, void* data, size_t size) const;

  inline const void* pgetv(size_t offset, size_t size) const {
    if (offset + size > this->length) {
      throw std::out_of_range("end of string");
    }
    return this->data + offset;
  }
  template <typename T>
  const T& pget(size_t offset, size_t size = sizeof(T)) const {
    return *reinterpret_cast<const T*>(this->pgetv(offset, size));
  }

  inline const void* getv(size_t size, bool advance = true) {
    const void* ret = this->pgetv(this->offset, size);
    if (advance) {
      this->offset += size;
    }
    return ret;
  }
  template <typename T>
  const T& get(bool advance = true, size_t size = sizeof(T)) {
    const T& ret = this->pget<T>(this->offset, size);
    if (advance) {
      this->offset += size;
    }
    return ret;
  }

  template <typename T>
  const T* pget_array(size_t offset, size_t count) {
    return reinterpret_cast<const T*>(this->pgetv(offset, count * sizeof(T)));
  }
  template <typename T>
  const T* get_array(size_t count, bool advance = true) {
    const T* ret = this->pget_array<T>(this->offset, count);
    if (advance) {
      this->offset += count * sizeof(T);
    }
    return ret;
  }

  inline uint8_t get_u8(bool advance = true) { return this->get<uint8_t>(advance); }
  inline int8_t get_s8(bool advance = true) { return this->get<int8_t>(advance); }
  inline uint8_t pget_u8(size_t offset) const { return this->pget<uint8_t>(offset); }
  inline int8_t pget_s8(size_t offset) const { return this->pget<int8_t>(offset); }

  inline uint16_t get_u16b(bool advance = true) { return this->get<be_uint16_t>(advance); }
  inline uint16_t get_u16l(bool advance = true) { return this->get<le_uint16_t>(advance); }
  inline int16_t get_s16b(bool advance = true) { return this->get<be_int16_t>(advance); }
  inline int16_t get_s16l(bool advance = true) { return this->get<le_int16_t>(advance); }
  inline uint16_t pget_u16b(size_t offset) const { return this->pget<be_uint16_t>(offset); }
  inline uint16_t pget_u16l(size_t offset) const { return this->pget<le_uint16_t>(offset); }
  inline int16_t pget_s16b(size_t offset) const { return this->pget<be_int16_t>(offset); }
  inline int16_t pget_s16l(size_t offset) const { return this->pget<le_int16_t>(offset); }

  inline uint32_t get_u32b(bool advance = true) { return this->get<be_uint32_t>(advance); }
  inline uint32_t get_u32l(bool advance = true) { return this->get<le_uint32_t>(advance); }
  inline int32_t get_s32b(bool advance = true) { return this->get<be_int32_t>(advance); }
  inline int32_t get_s32l(bool advance = true) { return this->get<le_int32_t>(advance); }
  inline uint32_t pget_u32b(size_t offset) const { return this->pget<be_uint32_t>(offset); }
  inline uint32_t pget_u32l(size_t offset) const { return this->pget<le_uint32_t>(offset); }
  inline int32_t pget_s32b(size_t offset) const { return this->pget<be_int32_t>(offset); }
  inline int32_t pget_s32l(size_t offset) const { return this->pget<le_int32_t>(offset); }

  inline uint64_t get_u64b(bool advance = true) { return this->get<be_uint64_t>(advance); }
  inline uint64_t get_u64l(bool advance = true) { return this->get<le_uint64_t>(advance); }
  inline int64_t get_s64b(bool advance = true) { return this->get<be_int64_t>(advance); }
  inline int64_t get_s64l(bool advance = true) { return this->get<le_int64_t>(advance); }
  inline uint64_t pget_u64b(size_t offset) const { return this->pget<be_uint64_t>(offset); }
  inline uint64_t pget_u64l(size_t offset) const { return this->pget<le_uint64_t>(offset); }
  inline int64_t pget_s64b(size_t offset) const { return this->pget<be_int64_t>(offset); }
  inline int64_t pget_s64l(size_t offset) const { return this->pget<le_int64_t>(offset); }

  inline float get_f32b(bool advance = true) { return this->get<be_float>(advance); }
  inline float get_f32l(bool advance = true) { return this->get<le_float>(advance); }
  inline float pget_f32b(size_t offset) const { return this->pget<be_float>(offset); }
  inline float pget_f32l(size_t offset) const { return this->pget<le_float>(offset); }

  inline double get_f64b(bool advance = true) { return this->get<be_double>(advance); }
  inline double get_f64l(bool advance = true) { return this->get<le_double>(advance); }
  inline double pget_f64b(size_t offset) const { return this->pget<be_double>(offset); }
  inline double pget_f64l(size_t offset) const { return this->pget<le_double>(offset); }

  inline uint32_t get_u24b(bool advance = true) {
    uint32_t ret = this->pget_u24b(this->offset);
    if (advance) {
      this->offset += 3;
    }
    return ret;
  }
  inline uint32_t get_u24l(bool advance = true) {
    uint32_t ret = this->pget_u24l(this->offset);
    if (advance) {
      this->offset += 3;
    }
    return ret;
  }
  inline int32_t get_s24b(bool advance = true) { return ext24(this->get_u24b(advance)); }
  inline int32_t get_s24l(bool advance = true) { return ext24(this->get_u24l(advance)); }

  inline uint32_t pget_u24b(size_t offset) const {
    if (offset + 3 > this->length) {
      throw std::out_of_range("end of string");
    }
    return (this->data[offset] << 16) | (this->data[offset + 1] << 8) | this->data[offset + 2];
  }
  inline uint32_t pget_u24l(size_t offset) const {
    if (offset + 3 > this->length) {
      throw std::out_of_range("end of string");
    }
    return this->data[offset] | (this->data[offset + 1] << 8) | (this->data[offset + 2] << 16);
  }
  inline int32_t pget_s24b(size_t offset) const { return ext24(this->pget_u24b(offset)); }
  inline int32_t pget_s24l(size_t offset) const { return ext24(this->pget_u24l(offset)); }

  inline uint64_t get_u48b(bool advance = true) {
    uint64_t ret = this->pget_u48b(this->offset);
    if (advance) {
      this->offset += 6;
    }
    return ret;
  }
  inline uint64_t get_u48l(bool advance = true) {
    uint64_t ret = this->pget_u48l(this->offset);
    if (advance) {
      this->offset += 6;
    }
    return ret;
  }
  inline int64_t get_s48b(bool advance = true) { return ext48(this->get_u48b(advance)); }
  inline int64_t get_s48l(bool advance = true) { return ext48(this->get_u48l(advance)); }
  inline uint64_t pget_u48b(size_t offset) const {
    if (offset + 6 > this->length) {
      throw std::out_of_range("end of string");
    }
    return (static_cast<uint64_t>(this->data[offset]) << 40) |
        (static_cast<uint64_t>(this->data[offset + 1]) << 32) |
        (static_cast<uint64_t>(this->data[offset + 2]) << 24) |
        (static_cast<uint64_t>(this->data[offset + 3]) << 16) |
        (static_cast<uint64_t>(this->data[offset + 4]) << 8) |
        (static_cast<uint64_t>(this->data[offset + 5]));
  }
  inline uint64_t pget_u48l(size_t offset) const {
    if (offset + 6 > this->length) {
      throw std::out_of_range("end of string");
    }
    return (static_cast<uint64_t>(this->data[offset])) |
        (static_cast<uint64_t>(this->data[offset + 1]) << 8) |
        (static_cast<uint64_t>(this->data[offset + 2]) << 16) |
        (static_cast<uint64_t>(this->data[offset + 3]) << 24) |
        (static_cast<uint64_t>(this->data[offset + 4]) << 32) |
        (static_cast<uint64_t>(this->data[offset + 5]) << 40);
  }
  inline int64_t pget_s48b(size_t offset) const { return ext48(this->pget_u48b(offset)); }
  inline int64_t pget_s48l(size_t offset) const { return ext48(this->pget_u48l(offset)); }

  std::string get_line(bool advance = true);

  std::string get_cstr(bool advance = true);
  std::string pget_cstr(size_t offset) const;

private:
  std::shared_ptr<std::string> owned_data;
  const uint8_t* data;
  size_t length;
  size_t offset;
};

class StringWriter {
public:
  StringWriter() = default;
  ~StringWriter() = default;

  void reset();

  inline void extend_to(size_t size, char v = '\0') {
    this->contents.resize(size, v);
  }
  inline void extend_by(size_t size, char v = '\0') {
    this->contents.resize(this->contents.size() + size, v);
  }

  void write(const void* data, size_t size);
  void write(const std::string& data);

  template <typename T>
  void put(const T& v) {
    this->contents.append(reinterpret_cast<const char*>(&v), sizeof(v));
  }

  template <typename T>
  void pput(size_t offset, const T& v) {
    if (offset + sizeof(T) > this->contents.size()) {
      this->contents.resize(offset + sizeof(T), '\0');
    }
    memcpy(this->contents.data() + offset, &v, sizeof(v));
  }

  inline void put_u8(uint8_t v) { this->put<uint8_t>(v); }
  inline void put_s8(int8_t v) { this->put<int8_t>(v); }
  inline void put_u16(uint16_t v) { this->put<uint16_t>(v); }
  inline void put_s16(int16_t v) { this->put<int16_t>(v); }
  inline void put_u32(uint32_t v) { this->put<uint32_t>(v); }
  inline void put_s32(int32_t v) { this->put<int32_t>(v); }
  inline void put_u64(uint64_t v) { this->put<uint64_t>(v); }
  inline void put_s64(int64_t v) { this->put<int64_t>(v); }
  inline void put_f32(float v) { this->put<float>(v); }
  inline void put_f64(double v) { this->put<double>(v); }

  inline void put_u16r(uint16_t v) { this->put<re_uint16_t>(v); }
  inline void put_s16r(int16_t v) { this->put<re_int16_t>(v); }
  inline void put_u32r(uint32_t v) { this->put<re_uint32_t>(v); }
  inline void put_s32r(int32_t v) { this->put<re_int32_t>(v); }
  inline void put_u64r(uint64_t v) { this->put<re_uint64_t>(v); }
  inline void put_s64r(int64_t v) { this->put<re_int64_t>(v); }
  inline void put_f32r(float v) { this->put<re_float>(v); }
  inline void put_f64r(double v) { this->put<re_double>(v); }

  inline void put_u16b(uint16_t v) { this->put<be_uint16_t>(v); }
  inline void put_s16b(int16_t v) { this->put<be_int16_t>(v); }
  inline void put_u32b(uint32_t v) { this->put<be_uint32_t>(v); }
  inline void put_s32b(int32_t v) { this->put<be_int32_t>(v); }
  inline void put_u64b(uint64_t v) { this->put<be_uint64_t>(v); }
  inline void put_s64b(int64_t v) { this->put<be_int64_t>(v); }
  inline void put_f32b(float v) { this->put<be_float>(v); }
  inline void put_f64b(double v) { this->put<be_double>(v); }

  inline void put_u16l(uint16_t v) { this->put<le_uint16_t>(v); }
  inline void put_s16l(int16_t v) { this->put<le_int16_t>(v); }
  inline void put_u32l(uint32_t v) { this->put<le_uint32_t>(v); }
  inline void put_s32l(int32_t v) { this->put<le_int32_t>(v); }
  inline void put_u64l(uint64_t v) { this->put<le_uint64_t>(v); }
  inline void put_s64l(int64_t v) { this->put<le_int64_t>(v); }
  inline void put_f32l(float v) { this->put<le_float>(v); }
  inline void put_f64l(double v) { this->put<le_double>(v); }

  inline void pput_u8(size_t offset, uint8_t v) { this->pput<uint8_t>(offset, v); }
  inline void pput_s8(size_t offset, int8_t v) { this->pput<int8_t>(offset, v); }
  inline void pput_u16(size_t offset, uint16_t v) { this->pput<uint16_t>(offset, v); }
  inline void pput_s16(size_t offset, int16_t v) { this->pput<int16_t>(offset, v); }
  inline void pput_u32(size_t offset, uint32_t v) { this->pput<uint32_t>(offset, v); }
  inline void pput_s32(size_t offset, int32_t v) { this->pput<int32_t>(offset, v); }
  inline void pput_u64(size_t offset, uint64_t v) { this->pput<uint64_t>(offset, v); }
  inline void pput_s64(size_t offset, int64_t v) { this->pput<int64_t>(offset, v); }
  inline void pput_f32(size_t offset, float v) { this->pput<float>(offset, v); }
  inline void pput_f64(size_t offset, double v) { this->pput<double>(offset, v); }

  inline void pput_u16r(size_t offset, uint16_t v) { this->pput<re_uint16_t>(offset, v); }
  inline void pput_s16r(size_t offset, int16_t v) { this->pput<re_int16_t>(offset, v); }
  inline void pput_u32r(size_t offset, uint32_t v) { this->pput<re_uint32_t>(offset, v); }
  inline void pput_s32r(size_t offset, int32_t v) { this->pput<re_int32_t>(offset, v); }
  inline void pput_u64r(size_t offset, uint64_t v) { this->pput<re_uint64_t>(offset, v); }
  inline void pput_s64r(size_t offset, int64_t v) { this->pput<re_int64_t>(offset, v); }
  inline void pput_f32r(size_t offset, float v) { this->pput<re_float>(offset, v); }
  inline void pput_f64r(size_t offset, double v) { this->pput<re_double>(offset, v); }

  inline void pput_u16b(size_t offset, uint16_t v) { this->pput<be_uint16_t>(offset, v); }
  inline void pput_s16b(size_t offset, int16_t v) { this->pput<be_int16_t>(offset, v); }
  inline void pput_u32b(size_t offset, uint32_t v) { this->pput<be_uint32_t>(offset, v); }
  inline void pput_s32b(size_t offset, int32_t v) { this->pput<be_int32_t>(offset, v); }
  inline void pput_u64b(size_t offset, uint64_t v) { this->pput<be_uint64_t>(offset, v); }
  inline void pput_s64b(size_t offset, int64_t v) { this->pput<be_int64_t>(offset, v); }
  inline void pput_f32b(size_t offset, float v) { this->pput<be_float>(offset, v); }
  inline void pput_f64b(size_t offset, double v) { this->pput<be_double>(offset, v); }

  inline void pput_u16l(size_t offset, uint16_t v) { this->pput<le_uint16_t>(offset, v); }
  inline void pput_s16l(size_t offset, int16_t v) { this->pput<le_int16_t>(offset, v); }
  inline void pput_u32l(size_t offset, uint32_t v) { this->pput<le_uint32_t>(offset, v); }
  inline void pput_s32l(size_t offset, int32_t v) { this->pput<le_int32_t>(offset, v); }
  inline void pput_u64l(size_t offset, uint64_t v) { this->pput<le_uint64_t>(offset, v); }
  inline void pput_s64l(size_t offset, int64_t v) { this->pput<le_int64_t>(offset, v); }
  inline void pput_f32l(size_t offset, float v) { this->pput<le_float>(offset, v); }
  inline void pput_f64l(size_t offset, double v) { this->pput<le_double>(offset, v); }

  inline size_t size() const {
    return this->contents.size();
  }
  inline void* data() {
    return this->contents.data();
  }
  inline const void* data() const {
    return this->contents.data();
  }
  inline std::string& str() {
    return this->contents;
  }
  inline const std::string& str() const {
    return this->contents;
  }

private:
  std::string contents;
};

class BufferWriter {
public:
  BufferWriter(void* buf, size_t buf_size) : buf(reinterpret_cast<uint8_t*>(buf)), buf_size(buf_size), offset(0) {}
  ~BufferWriter() = default;

  inline void pwrite(size_t offset, const void* data, size_t size) {
    if (offset + size > this->buf_size) {
      throw std::runtime_error("Offset out of bounds");
    }
    memcpy(this->buf + offset, data, size);
  }
  inline void pwrite(size_t offset, const std::string& data) {
    this->pwrite(offset, data.data(), data.size());
  }

  inline void write(const void* data, size_t size) {
    this->pwrite(this->offset, data, size);
    this->offset += size;
  }
  inline void write(const std::string& data) {
    this->write(data.data(), data.size());
  }

  template <typename T>
  void put(const T& v) {
    this->write(&v, sizeof(v));
  }
  template <typename T>
  void pput(size_t offset, const T& v) {
    this->pwrite(offset, &v, sizeof(v));
  }

  inline void put_u8(uint8_t v) { this->put<uint8_t>(v); }
  inline void put_s8(int8_t v) { this->put<int8_t>(v); }
  inline void put_u16(uint16_t v) { this->put<uint16_t>(v); }
  inline void put_s16(int16_t v) { this->put<int16_t>(v); }
  inline void put_u32(uint32_t v) { this->put<uint32_t>(v); }
  inline void put_s32(int32_t v) { this->put<int32_t>(v); }
  inline void put_u64(uint64_t v) { this->put<uint64_t>(v); }
  inline void put_s64(int64_t v) { this->put<int64_t>(v); }
  inline void put_f32(float v) { this->put<float>(v); }
  inline void put_f64(double v) { this->put<double>(v); }

  inline void put_u16r(uint16_t v) { this->put<re_uint16_t>(v); }
  inline void put_s16r(int16_t v) { this->put<re_int16_t>(v); }
  inline void put_u32r(uint32_t v) { this->put<re_uint32_t>(v); }
  inline void put_s32r(int32_t v) { this->put<re_int32_t>(v); }
  inline void put_u64r(uint64_t v) { this->put<re_uint64_t>(v); }
  inline void put_s64r(int64_t v) { this->put<re_int64_t>(v); }
  inline void put_f32r(float v) { this->put<re_float>(v); }
  inline void put_f64r(double v) { this->put<re_double>(v); }

  inline void put_u16b(uint16_t v) { this->put<be_uint16_t>(v); }
  inline void put_s16b(int16_t v) { this->put<be_int16_t>(v); }
  inline void put_u32b(uint32_t v) { this->put<be_uint32_t>(v); }
  inline void put_s32b(int32_t v) { this->put<be_int32_t>(v); }
  inline void put_u64b(uint64_t v) { this->put<be_uint64_t>(v); }
  inline void put_s64b(int64_t v) { this->put<be_int64_t>(v); }
  inline void put_f32b(float v) { this->put<be_float>(v); }
  inline void put_f64b(double v) { this->put<be_double>(v); }

  inline void put_u16l(uint16_t v) { this->put<le_uint16_t>(v); }
  inline void put_s16l(int16_t v) { this->put<le_int16_t>(v); }
  inline void put_u32l(uint32_t v) { this->put<le_uint32_t>(v); }
  inline void put_s32l(int32_t v) { this->put<le_int32_t>(v); }
  inline void put_u64l(uint64_t v) { this->put<le_uint64_t>(v); }
  inline void put_s64l(int64_t v) { this->put<le_int64_t>(v); }
  inline void put_f32l(float v) { this->put<le_float>(v); }
  inline void put_f64l(double v) { this->put<le_double>(v); }

  inline void pput_u8(size_t offset, uint8_t v) { this->pput<uint8_t>(offset, v); }
  inline void pput_s8(size_t offset, int8_t v) { this->pput<int8_t>(offset, v); }
  inline void pput_u16(size_t offset, uint16_t v) { this->pput<uint16_t>(offset, v); }
  inline void pput_s16(size_t offset, int16_t v) { this->pput<int16_t>(offset, v); }
  inline void pput_u32(size_t offset, uint32_t v) { this->pput<uint32_t>(offset, v); }
  inline void pput_s32(size_t offset, int32_t v) { this->pput<int32_t>(offset, v); }
  inline void pput_u64(size_t offset, uint64_t v) { this->pput<uint64_t>(offset, v); }
  inline void pput_s64(size_t offset, int64_t v) { this->pput<int64_t>(offset, v); }
  inline void pput_f32(size_t offset, float v) { this->pput<float>(offset, v); }
  inline void pput_f64(size_t offset, double v) { this->pput<double>(offset, v); }

  inline void pput_u16r(size_t offset, uint16_t v) { this->pput<re_uint16_t>(offset, v); }
  inline void pput_s16r(size_t offset, int16_t v) { this->pput<re_int16_t>(offset, v); }
  inline void pput_u32r(size_t offset, uint32_t v) { this->pput<re_uint32_t>(offset, v); }
  inline void pput_s32r(size_t offset, int32_t v) { this->pput<re_int32_t>(offset, v); }
  inline void pput_u64r(size_t offset, uint64_t v) { this->pput<re_uint64_t>(offset, v); }
  inline void pput_s64r(size_t offset, int64_t v) { this->pput<re_int64_t>(offset, v); }
  inline void pput_f32r(size_t offset, float v) { this->pput<re_float>(offset, v); }
  inline void pput_f64r(size_t offset, double v) { this->pput<re_double>(offset, v); }

  inline void pput_u16b(size_t offset, uint16_t v) { this->pput<be_uint16_t>(offset, v); }
  inline void pput_s16b(size_t offset, int16_t v) { this->pput<be_int16_t>(offset, v); }
  inline void pput_u32b(size_t offset, uint32_t v) { this->pput<be_uint32_t>(offset, v); }
  inline void pput_s32b(size_t offset, int32_t v) { this->pput<be_int32_t>(offset, v); }
  inline void pput_u64b(size_t offset, uint64_t v) { this->pput<be_uint64_t>(offset, v); }
  inline void pput_s64b(size_t offset, int64_t v) { this->pput<be_int64_t>(offset, v); }
  inline void pput_f32b(size_t offset, float v) { this->pput<be_float>(offset, v); }
  inline void pput_f64b(size_t offset, double v) { this->pput<be_double>(offset, v); }

  inline void pput_u16l(size_t offset, uint16_t v) { this->pput<le_uint16_t>(offset, v); }
  inline void pput_s16l(size_t offset, int16_t v) { this->pput<le_int16_t>(offset, v); }
  inline void pput_u32l(size_t offset, uint32_t v) { this->pput<le_uint32_t>(offset, v); }
  inline void pput_s32l(size_t offset, int32_t v) { this->pput<le_int32_t>(offset, v); }
  inline void pput_u64l(size_t offset, uint64_t v) { this->pput<le_uint64_t>(offset, v); }
  inline void pput_s64l(size_t offset, int64_t v) { this->pput<le_int64_t>(offset, v); }
  inline void pput_f32l(size_t offset, float v) { this->pput<le_float>(offset, v); }
  inline void pput_f64l(size_t offset, double v) { this->pput<le_double>(offset, v); }

private:
  uint8_t* buf;
  size_t buf_size;
  size_t offset;
};

class BlockStringWriter {
public:
  BlockStringWriter() = default;
  ~BlockStringWriter() = default;

  void write(const void* data, size_t size);
  void write(const std::string& data);
  void write(std::string&& data);

  template <typename T>
  void put(const T& v) {
    this->write(&v, sizeof(v));
  }

  std::string close(const char* separator = "");

  template <typename... ArgTs>
  void write_fmt(std::format_string<ArgTs...> fmt, ArgTs&&... args) {
    this->write(std::format(fmt, std::forward<ArgTs>(args)...));
  }

private:
  std::deque<std::string> blocks;
};

template <typename T>
class StringBuffer : std::string {
public:
  StringBuffer(size_t size = sizeof(T)) : std::string(size, '\0') {}
  virtual ~StringBuffer() = default;

  T* buffer() {
    return reinterpret_cast<T*>(this->data());
  }
};

template <typename T>
T* data_at(std::string& s, size_t offset = 0) {
  return reinterpret_cast<T*>(s.data() + offset);
}

size_t count_zeroes(const void* vdata, size_t size, size_t stride = 1);

} // namespace phosg
