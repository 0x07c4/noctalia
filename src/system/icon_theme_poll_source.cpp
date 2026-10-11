#include "system/icon_theme_poll_source.h"

#include "system/icon_resolver.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <sys/inotify.h>
#include <unordered_set>

namespace fs = std::filesystem;

namespace {
  constexpr std::uint32_t kWatchMask =
      IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF | IN_ATTRIB;

  std::optional<std::string> resolvedDirectory(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
      return std::nullopt;
    }
    const fs::path resolved = fs::weakly_canonical(path, ec);
    return ec ? std::optional<std::string>{path.lexically_normal().string()} : resolved.string();
  }
} // namespace

int IconThemePollSource::pollTimeoutMs() const {
  const auto now = Clock::now();
  if (now >= m_nextCheck) {
    return 0;
  }
  return static_cast<int>(std::chrono::ceil<std::chrono::milliseconds>(m_nextCheck - now).count());
}

void IconThemePollSource::dispatch(const std::vector<pollfd>& fds, std::size_t startIdx) {
  const auto now = Clock::now();
  const bool watchReady = m_inotify.fd() >= 0 && startIdx < fds.size() && (fds[startIdx].revents & POLLIN) != 0;

  if (watchReady && drainWatchEvents()) {
    m_nextCheck = now + kCheckInterval;
    publishChange();
    return;
  }

  if (now < m_nextCheck) {
    return;
  }

  m_nextCheck = now + kCheckInterval;
  if (IconResolver::checkThemeChanged()) {
    reconcileWatches();
    if (m_changeCallback) {
      m_changeCallback();
    }
  }
}

void IconThemePollSource::doAddPollFds(std::vector<pollfd>& fds) {
  if (!m_watchesInitialized) {
    reconcileWatches();
    m_watchesInitialized = true;
  }
  if (m_inotify.fd() >= 0) {
    fds.push_back({.fd = m_inotify.fd(), .events = POLLIN, .revents = 0});
  }
}

void IconThemePollSource::reconcileWatches() {
  std::unordered_map<std::string, WatchEntry> desired;
  const auto paths = IconResolver::themeWatchPaths();

  for (const auto& directory : paths.directories) {
    if (const auto resolved = resolvedDirectory(directory); resolved.has_value()) {
      desired[*resolved].allEntries = true;
    }
  }

  for (const auto& file : paths.files) {
    const fs::path filePath(file);
    if (const auto parent = resolvedDirectory(filePath.parent_path()); parent.has_value()) {
      desired[*parent].filenames.insert(filePath.filename().string());
    }
  }

  for (auto it = m_descriptorsByPath.begin(); it != m_descriptorsByPath.end();) {
    if (desired.contains(it->first)) {
      ++it;
      continue;
    }
    m_inotify.unwatch(it->second);
    m_watchesByDescriptor.erase(it->second);
    it = m_descriptorsByPath.erase(it);
  }

  for (auto& [path, entry] : desired) {
    const auto existing = m_descriptorsByPath.find(path);
    if (existing != m_descriptorsByPath.end()) {
      m_watchesByDescriptor[existing->second] = std::move(entry);
      continue;
    }

    const auto descriptor = m_inotify.watch(path, kWatchMask);
    if (!descriptor.has_value()) {
      continue;
    }
    m_descriptorsByPath.emplace(path, *descriptor);
    m_watchesByDescriptor.emplace(*descriptor, std::move(entry));
  }
}

bool IconThemePollSource::drainWatchEvents() {
  bool changed = false;
  std::unordered_set<int> invalidated;
  m_inotify.drain([this, &changed, &invalidated](const inotify_event* event) {
    if ((event->mask & IN_Q_OVERFLOW) != 0) {
      changed = true;
      return;
    }

    const auto watch = m_watchesByDescriptor.find(event->wd);
    if (watch == m_watchesByDescriptor.end()) {
      return;
    }
    if ((event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) != 0) {
      invalidated.insert(event->wd);
      changed = true;
      return;
    }
    if (watch->second.allEntries) {
      changed = true;
      return;
    }
    if (event->len > 0 && watch->second.filenames.contains(event->name)) {
      changed = true;
    }
  });

  for (const int descriptor : invalidated) {
    m_watchesByDescriptor.erase(descriptor);
    std::erase_if(m_descriptorsByPath, [descriptor](const auto& entry) { return entry.second == descriptor; });
  }
  return changed;
}

void IconThemePollSource::publishChange() {
  IconResolver::refreshTheme();
  reconcileWatches();
  if (m_changeCallback) {
    m_changeCallback();
  }
}
