#include "analyzer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>

namespace wlan {
namespace {

constexpr int kWeakRssiThreshold = -75;   // 低于该值判定为弱信号
constexpr int kBucketSize        = 5;     // RSSI 直方图分桶宽度 dBm

}  // namespace

void LinkAnalyzer::Accept(const ParseResult& result) {
    ++stats_.totalLines;

    switch (result.status) {
        case FrameStatus::kOk:
            ++stats_.validFrames;
            AcceptValid(result.frame);
            break;
        case FrameStatus::kCrcError:
            ++stats_.crcErrors;
            break;
        case FrameStatus::kFormatError:
            ++stats_.formatErrors;
            break;
    }
}

void LinkAnalyzer::AcceptValid(const Frame& frame) {
    // --- 信号强度 ---
    int rssi = frame.rssi;
    stats_.rssiSum += rssi;
    stats_.rssiMin  = std::min(stats_.rssiMin, rssi);
    stats_.rssiMax  = std::max(stats_.rssiMax, rssi);
    if (rssi < kWeakRssiThreshold) ++stats_.weakRssiFrames;
    ++rssiHistogram_[(rssi / kBucketSize) * kBucketSize];

    // --- 有效载荷，用于算吞吐 ---
    stats_.payloadBytes += static_cast<long>(frame.payload.size());

    // --- 首帧用于确定时间基准 ---
    if (!hasPrev_) {
        firstTs_   = frame.timestampMs;
        hasPrev_   = true;
        prevTs_    = frame.timestampMs;
        prevSeq_   = frame.seq;
        expected_  = 1;
        return;
    }
    lastTs_ = frame.timestampMs;
    ++expected_;

    // --- 记录帧间隔，抖动在 Build 阶段统一计算 ---
    double interval = frame.timestampMs - prevTs_;
    if (interval >= 0.0) intervals_.push_back(interval);

    // --- 序列号连续性：判丢包与重帧 ---
    int delta = static_cast<int>(frame.seq) - static_cast<int>(prevSeq_);
    if (delta < 0) delta += 256;            // SEQ 是 8 位，会回绕
    if (delta == 0) {
        ++stats_.duplicateSeq;
    } else if (delta > 1 && delta < 100) {  // 跳号即认为中间丢了帧
        stats_.lostPackets += delta - 1;
    }

    prevTs_  = frame.timestampMs;
    prevSeq_ = frame.seq;
}

LinkStats LinkAnalyzer::Build(std::ostream& os) const {
    LinkStats s = stats_;

    long decided = s.totalLines;
    if (decided > 0) {
        s.errorRate = 100.0 * (s.crcErrors + s.formatErrors) / decided;
    }

    if (expected_ > 0) {
        s.lossRate = 100.0 * s.lostPackets / (expected_ + s.lostPackets);
    }
    if (s.validFrames > 0) {
        s.rssiAvg        = static_cast<double>(s.rssiSum) / s.validFrames;
        s.weakRate       = 100.0 * s.weakRssiFrames / s.validFrames;
    }
    if (!intervals_.empty()) {
        double sum = 0.0;
        for (double t : intervals_) sum += t;
        s.intervalAvg = sum / static_cast<double>(intervals_.size());
        // 抖动 = 每一帧间隔偏离平均值的最大幅度
        double maxDeviation = 0.0;
        for (double t : intervals_) {
            maxDeviation = std::max(maxDeviation, std::abs(t - s.intervalAvg));
        }
        s.jitterMax = maxDeviation;
    }
    s.durationMs     = (hasPrev_ ? (lastTs_ - firstTs_) : 0.0);
    s.throughputKbps = (s.durationMs > 0.0)
                           ? (s.payloadBytes * 8.0 / s.durationMs)
                           : 0.0;
    (void)os;   // Build 不输出，保留 os 仅为接口统一
    return s;
}

LinkStats LinkAnalyzer::PrintReport(std::ostream& os) const {
    LinkStats s = Build(os);

    os << "================ 无线链路质量分析报告 ================\n";
    os << "抓包行数        : " << s.totalLines << " 行\n";
    os << "有效帧数        : " << s.validFrames << " 帧\n";
    os << "CRC 错帧        : " << s.crcErrors << " 帧\n";
    os << "格式非法帧      : " << s.formatErrors << " 帧\n";
    os << "丢帧（按序列号推算）: " << s.lostPackets << " 帧\n";
    os << "重复帧          : " << s.duplicateSeq << " 帧\n";
    os << "------------------------------------------------------\n";
    os.setf(std::ios::fixed);
    os << std::setprecision(2);
    os << "错帧率          : " << s.errorRate << " %\n";
    os << "丢包率          : " << s.lossRate  << " %\n";
    os << "平均帧间隔      : " << s.intervalAvg << " ms\n";
    os << "最大抖动        : " << s.jitterMax << " ms\n";
    os << std::setprecision(1);
    os << "RSSI 平均/最弱/最强 : " << s.rssiAvg << " / " << s.rssiMin
       << " / " << s.rssiMax << " dBm\n";
    os << "弱信号帧占比    : " << s.weakRate << " %\n";
    os << std::setprecision(3);
    os << "统计时长        : " << s.durationMs << " ms\n";
    os << "有效吞吐        : " << s.throughputKbps << " kbps\n";
    os << "------------------------------------------------------\n";
    os << "RSSI 分布（每 5dBm 一档）:\n";
    for (const auto& kv : rssiHistogram_) {
        os << "  [" << std::setw(4) << kv.first << ", " << kv.first + kBucketSize
           << ") dBm : ";
        os << std::string(static_cast<std::size_t>(kv.second), '#')
           << " " << kv.second << "\n";
    }
    os << "======================================================\n";
    return s;
}

}  // namespace wlan
