#pragma once

#include "app/poll_source.h"

#include <array>
#include <chrono>
#include <functional>
#include <optional>
#include <string>

namespace compositors::umbriel {

  class UmbrielRuntime;

  class UmbrielAudioProvider final : public PollSource {
  public:
    using Bands = std::array<float, 64>;
    using SnapshotSource = std::function<Bands()>;
    UmbrielAudioProvider(UmbrielRuntime& runtime, SnapshotSource source);
    ~UmbrielAudioProvider() override;
    UmbrielAudioProvider(const UmbrielAudioProvider&) = delete;
    UmbrielAudioProvider& operator=(const UmbrielAudioProvider&) = delete;

    void setEnabled(bool enabled);
    void notifyChanged();
    [[nodiscard]] int pollTimeoutMs() const override;
    void dispatch(const std::vector<pollfd>& fds, std::size_t startIdx) override;

  protected:
    void doAddPollFds(std::vector<pollfd>& fds) override;

  private:
    using Clock = std::chrono::steady_clock;
    void disconnect(bool retry);
    void connectSocket();
    bool readReply();
    bool writeSnapshot();

    UmbrielRuntime& m_runtime;
    SnapshotSource m_source;
    bool m_enabled = false;
    bool m_connecting = false;
    bool m_awaitingReply = false;
    int m_fd = -1;
    std::string m_output;
    std::size_t m_written = 0;
    std::string m_input;
    std::optional<Bands> m_lastSent;
    Clock::time_point m_nextCheckAt;
    Clock::time_point m_nextConnectAt;
    Clock::time_point m_deadline;
    Clock::time_point m_lastSentAt;
    std::chrono::milliseconds m_backoff{100};
  };

} // namespace compositors::umbriel
