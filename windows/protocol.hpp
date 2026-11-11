#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace paw {

inline std::vector<std::string> fields(std::string_view line) {
  std::vector<std::string> result;
  while (true) {
    const size_t tab = line.find('\t');
    result.emplace_back(line.substr(0, tab));
    if (tab == std::string_view::npos) return result;
    line.remove_prefix(tab + 1);
  }
}

struct Task {
  std::string outcome, scope, total, input, output, cached, cny, usd, coverage, mode;
  bool incomplete{};
};

struct Wallet { std::string currency, total, bonus, cash; };

struct Balance {
  std::string status{"idle"}, mode, available;
  std::vector<Wallet> wallets;
};

inline std::wstring ascii(std::string_view value) { return {value.begin(), value.end()}; }

inline std::wstring grouped(const std::string& number) {
  if (number.empty() || number == "-") return L"—";
  std::wstring result;
  for (size_t i = 0; i < number.size(); ++i) {
    if (i > 0 && (number.size() - i) % 3 == 0) result += L',';
    result += static_cast<wchar_t>(number[i]);
  }
  return result;
}

inline std::wstring money(std::string_view currency, const std::string& value) {
  if (value.empty() || value == "-") return L"—";
  const std::wstring symbol = currency == "USD" ? L"$" : L"¥";
  return value.front() == '<' ? L"<" + symbol + ascii(std::string_view(value).substr(1)) : symbol + ascii(value);
}

inline std::wstring taskTitle(const Task& task) {
  if (task.outcome == "completed") return task.scope == "child" ? L"子任务完成" : L"任务完成";
  if (task.outcome == "limit") return L"已达输出上限";
  if (task.outcome == "cancelled") return L"任务已中断";
  if (task.outcome == "blocked") return L"任务未执行";
  return L"任务未完成";
}

inline std::wstring taskPrice(const Task& task, const Balance& balance) {
  const bool useUSD = task.mode == balance.mode && balance.wallets.size() == 1
    && balance.wallets.front().currency == "USD";
  const std::string& value = useUSD ? task.usd : task.cny;
  if (value.empty() || value == "-") return L"费用待定";
  return (task.coverage == "partial" ? L"部分 " : L"预估 ") + money(useUSD ? "USD" : "CNY", value);
}

} // namespace paw
