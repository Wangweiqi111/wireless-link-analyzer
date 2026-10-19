#ifndef ANALYZER_H
#define ANALYZER_H

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "frame.h"

namespace wlan {

// 链路质量统计结果
struct LinkStats {
    long   totalLines     = 0;   // 抓到的行（帧候选）总数
    long   validFrames    = 0;   // 通过 CRC 的有效帧
    long   crcErrors      = 0;   // CRC 错帧
    long   formatErrors   = 0;   // 格式非法帧
    long   lostPackets    = 0;   // 依据序列号跳变推算的丢帧数
    long   duplicateSeq   = 0;   // 重复帧
    long   weakRssiFrames = 0;   // RSSI < -75dBm 的弱信号帧
    long   payloadBytes   = 0;   // 有效载荷总字节数
    double errorRate      = 0.0; // 错帧率 %
    double lossRate       = 0.0; // 丢包率 %
    double weakRate       = 0.0; // 弱信号占比 %
    double rssiAvg        = 0.0;
    int    rssiMin        = 0;
    int    rssiMax        = -127;
    double rssiSum        = 0.0;  // 累加中间量，用于算均值
    double intervalAvg    = 0.0; // 平均帧间隔 ms
    double jitterMax      = 0.0; // 最大抖动 ms
    double durationMs     = 0.0;
    double throughputKbps = 0.0;
};

/*
 * 逐帧喂入解析结果，内部累积统计状态。
 * 设计上保持单一职责：本类只负责统计，不负责解析和 IO。
 */
class LinkAnalyzer {
public:
    void Accept(const ParseResult& result);
    LinkStats Build(std::ostream& os) const;      // 计算派生指标
    LinkStats PrintReport(std::ostream& os) const; // 生成可读的测试报告

private:
    void AcceptValid(const Frame& frame);

    LinkStats stats_;
    bool   hasPrev_     = false;
    double prevTs_      = 0.0;
    std::uint8_t prevSeq_ = 0;
    long   expected_    = 0;   // 依据首个序列号推算应收到的帧数
    double firstTs_     = 0.0;
    double lastTs_      = 0.0;
    std::vector<double> intervals_;          // 每两帧之间的时间间隔，用于算抖动
    std::map<int, int>  rssiHistogram_;      // 按 5dBm 分桶统计信号分布
};

}  // namespace wlan

#endif  // ANALYZER_H
