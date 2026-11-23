#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace oopagent {

enum class LoopKind { None, GenericRepeat, PingPong };

struct LoopStatus {
    LoopKind kind = LoopKind::None;
    std::size_t repetitions = 0;
    bool warning = false;
    bool critical = false;
};

class LoopDetector {
public:
    explicit LoopDetector(std::size_t warning_threshold = 2,
                          std::size_t critical_threshold = 4);
    LoopStatus observe(const std::string& action_signature);

private:
    std::size_t warning_threshold_;
    std::size_t critical_threshold_;
    std::vector<std::string> history_;
};

const char* loop_kind_name(LoopKind kind);

} // namespace oopagent
