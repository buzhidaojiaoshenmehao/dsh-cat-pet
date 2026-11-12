#pragma once

#include <optional>
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

inline std::optional<Task> parseTask(const std::vector<std::string>& packet) {
  if (packet.size() != 14) return std::nullopt;
  return Task{packet[1], packet[2], packet[3], packet[4], packet[5], packet[6],
    packet[7], packet[8], packet[9], packet[13], packet[10] == "1"};
}

inline std::optional<Balance> parseBalance(const std::vector<std::string>& packet) {
  if (packet.size() < 2) return std::nullopt;
  Balance result;
  result.status = packet[1];
  if (result.status != "ready") return result;
  if (packet.size() != 8 && packet.size() != 12) return std::nullopt;
  result.mode = packet[2];
  result.available = packet[3];
  for (size_t i = 4; i < packet.size(); i += 4) {
    result.wallets.push_back({packet[i], packet[i + 1], packet[i + 2], packet[i + 3]});
  }
  return result;
}

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

inline std::wstring taskCostNote(const Task& task) {
  if (task.incomplete) return L"部分用量未返回";
  if (task.coverage == "full") return L"模型费用按官方峰谷价估算";
  if (task.coverage == "partial") return L"仅显示可计价的模型费用";
  return L"当前模型费用无可用价格";
}

inline std::wstring tokenText(const Task& task) {
  if (task.total == "-") return L"用量未返回";
  return (task.incomplete ? L"已知 " : L"") + grouped(task.total) + L" Token";
}

inline std::wstring balanceMessage(const std::string& status) {
  if (status == "loading") return L"查询余额中…";
  if (status == "sign_in") return L"请先在 Harness 中登录 DeepSeek 账号";
  if (status == "no_key") return L"请在 Harness 的模型设置中配置 API Key";
  if (status == "auth") return L"DeepSeek 拒绝了 API Key，请检查模型设置";
  if (status == "custom") return L"请到自定义端点的服务商查看余额";
  if (status == "empty") return L"当前账号没有可显示的钱包";
  if (status == "idle") return L"点刷新查询余额";
  return L"余额查询失败，可点刷新重试";
}

} // namespace paw
