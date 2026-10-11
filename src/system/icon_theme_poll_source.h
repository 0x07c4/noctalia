#pragma once

#include "app/poll_source.h"
#include "core/inotify/inotify.h"

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class IconThemePollSource final : public PollSource {
private:
  using Clock = std::chrono::steady_clock;

  struct WatchEntry {
    bool allEntries = false;
    std::unordered_set<std::string> filenames;
  };

  static constexpr std::chrono::seconds kCheckInterval{60};

public:
  void setChangeCallback(std::function<void()> callback) { m_changeCallback = std::move(callback); }

  [[nodiscard]] int pollTimeoutMs() const override;
  void dispatch(const std::vector<pollfd>& fds, std::size_t startIdx) override;

protected:
  void doAddPollFds(std::vector<pollfd>& fds) override;

private:
  void reconcileWatches();
  bool drainWatchEvents();
  void publishChange();

  Inotify m_inotify;
  std::function<void()> m_changeCallback;
  std::unordered_map<int, WatchEntry> m_watchesByDescriptor;
  std::unordered_map<std::string, int> m_descriptorsByPath;
  Clock::time_point m_nextCheck = Clock::now() + kCheckInterval;
  bool m_watchesInitialized = false;
};
