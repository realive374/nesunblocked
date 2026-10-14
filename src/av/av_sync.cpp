#include "av/av_sync.h"

#include <algorithm>

namespace rt90 {

double AvSync::adjust(double wall_dt, size_t queued_samples) {
    const double q = static_cast<double>(queued_samples);
    filtered_ = filtered_ < 0 ? q : filtered_ + 0.05 * (q - filtered_);  // smooth out callback jitter
    const double error = (target_ - filtered_) / target_;  // >0: queue too short -> run faster
    ratio_ = 1.0 + std::clamp(error * 0.01, -kMaxAdjust, kMaxAdjust);
    return wall_dt * ratio_;
}

}  // namespace rt90
