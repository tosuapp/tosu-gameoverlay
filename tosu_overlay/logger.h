#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>

namespace logger {

namespace {

std::string log_path;

template <typename... Args>
std::string _format(const std::string_view format, Args... args) {
  const auto size = std::snprintf(nullptr, 0, format.data(), args...) + 1;

  if (size <= 0) {
    return {};
  }

  const auto buffer = std::make_unique<char[]>(size);
  std::snprintf(buffer.get(), size, format.data(), args...);

  return std::string(buffer.get(), size - 1);
}

// TODO: ofstream could be re-used but whatever for now
void _log(std::string_view text) {
  std::ofstream out_file;

  out_file.open(log_path, std::ios_base::app);
  out_file << text;
  out_file << "\n";
}

}  // namespace

inline void setup_logger(std::filesystem::path path) {
  const auto prefix = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

  const auto file_name = _format("tosu_gameoverlay_%lld.log", prefix);

  log_path = (path / file_name).string();
}

template <typename... T>
inline void log(std::string_view format, T&&... args) {
  _log(_format(format, args...));
}

}  // namespace logger