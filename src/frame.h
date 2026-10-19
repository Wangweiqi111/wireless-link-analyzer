#ifndef FRAME_H
#define FRAME_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wlan {

// 一帧数据的解析结果状态
enum class FrameStatus {
    kOk,           // 解析成功且 CRC 校验通过
    kCrcError,     // 帧格式正确，但 CRC 校验失败（空中受干扰）
    kFormatError   // 长度不足 / 帧头不对 / 十六进制非法
};

// 解析出来的一帧：类里的成员即为空口帧的字段
struct Frame {
    double              timestampMs = 0.0;   // 抓包时间戳（毫秒）
    std::uint8_t        type        = 0;     // 帧类型：0x10 数据帧，0x20 应答帧
    std::uint8_t        seq         = 0;     // 序列号，用于判丢包
    std::int8_t         rssi        = 0;     // 信号强度 dBm（负值）
    std::vector<std::uint8_t> payload;       // 有效载荷
};

// 每一行抓包数据的解析结果
struct ParseResult {
    FrameStatus status  = FrameStatus::kFormatError;
    Frame       frame;
    std::string message;                     // 出错原因，便于定位问题
};

// 把 "AA55 10 0A" 或 "AA55100A" 这类文本转成字节数组
std::vector<std::uint8_t> ParseHex(const std::string& text);

// CRC-16/CCITT-FALSE，覆盖范围 TYPE ~ PAYLOAD（与常见无线协议一致）
std::uint16_t Crc16Ccitt(const std::uint8_t* data, std::size_t len);

/*
 * 解析一行日志，格式：<时间戳ms> <十六进制帧>
 * 帧结构：SOF(AA55) | TYPE(1B) | SEQ(1B) | RSSI(1B) | LEN(1B) | PAYLOAD(NB) | CRC16(2B, 小端)
 */
ParseResult ParseLine(const std::string& line);

}  // namespace wlan

#endif  // FRAME_H
