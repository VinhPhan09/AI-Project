#include "loop_detector.hpp"

#include <stdexcept>

namespace oopagent {

LoopDetector::LoopDetector(std::size_t warning_threshold, std::size_t critical_threshold)
    : warning_threshold_(warning_threshold), critical_threshold_(critical_threshold) {
    if (warning_threshold_ < 2 || critical_threshold_ <= warning_threshold_) {
        throw std::invalid_argument("Loop thresholds require warning >= 2 and critical > warning");
    }
}

LoopStatus LoopDetector::observe(const std::string& action_signature) {
    history_.push_back(action_signature);

    std::size_t repeated = 1;
    for (std::size_t index = history_.size() - 1; index > 0; --index) {
        if (history_[index - 1] != action_signature) break;
        ++repeated;
    }
    if (repeated >= warning_threshold_) {
        return {LoopKind::GenericRepeat, repeated, true, repeated >= critical_threshold_};
    }

    if (history_.size() < 4) return {};
    const std::size_t last = history_.size() - 1;
    if (history_[last] == history_[last - 1]) return {};

    std::size_t alternating_actions = 2;
    while (alternating_actions < history_.size()) {
        const std::size_t current = history_.size() - 1 - alternating_actions;
        const std::size_t two_later = current + 2;
        if (history_[current] != history_[two_later]) break;
        ++alternating_actions;
    }
    const std::size_t cycles = alternating_actions / 2;
    if (cycles >= warning_threshold_) {
        return {LoopKind::PingPong, cycles, true, cycles >= critical_threshold_};
    }
    return {};
}

const char* loop_kind_name(LoopKind kind) {
    switch (kind) {
    case LoopKind::GenericRepeat: return "generic repeat";
    case LoopKind::PingPong: return "ping-pong";
    default: return "none";
    }
}

} // namespace oopagent
