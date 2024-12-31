#pragma once
#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>

const auto compute_min = [](const std::vector<double>& v) -> double { return *std::min_element(v.begin(), v.end()); };
const auto compute_max = [](const std::vector<double>& v) -> double { return *std::max_element(v.begin(), v.end()); };

const auto format_number = [](double number) -> std::string {
  constexpr size_t kIs1024 = 1024;
  const char* suffixes[] = { "", "K", "M", "B", "T" };

  int index = 0;
  while (number >= kIs1024 && index < 4) {
    number /= kIs1024;
    index++;
  }

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << number << suffixes[index];
  return oss.str();
};

const auto format_bytes = [](size_t bytes) -> std::string {
  const char* suffixes[] = { "B", "KB", "MB", "GB" };
  int index = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024 && index < 3) {
    size /= 1024;
    index++;
  }

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << size << suffixes[index];
  return oss.str();
};
