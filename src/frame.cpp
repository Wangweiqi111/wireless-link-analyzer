#include "frame.h"

#include <cctype>
#include <sstream>

namespace wlan {
namespace {

constexpr std::uint8_t kSofHigh = 0xAA;
constexpr std::uint8_t kSofLow  = 0x55;
constexpr std::size_t  kHeaderBytes = 5;   // SOF(2) + TYPE(1) + SEQ(1) + RSSI(1)
constexpr std::size_t  kLenIndex    = 5;   // LEN 字段紧跟在 RSSI 之后
constexpr std::size_t  kPayloadPos  = 6;   // PAYLOAD 起始下标
constexpr std::size_t  kCrcBytes    = 2;

// 把一个十六进制字符转成数值，非法字符返回 -1
int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

std::vector<std::uint8_t> ParseHex(const std::string& text) {
    std::vector<std::uint8_t> bytes;
    int  high = -1;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;   // 忽略空格分隔符
        int value = HexValue(c);
        if (value < 0) return {};                                    // 出现非法字符，整行作废
        if (high < 0) {
            high = value;
        } else {
            bytes.push_back(static_cast<std::uint8_t>(high * 16 + value));
            high = -1;
        }
    }
    if (high >= 0) return {};   // 奇数个十六进制字符，数据不完整
    return bytes;
}

std::uint16_t Crc16Ccitt(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<std::uint16_t>(crc << 1);
        }
    }
    return crc;
}

ParseResult ParseLine(const std::string& line) {
    ParseResult result;

    // 1) 拆出时间戳和十六进制正文
    std::istringstream stream(line);
    std::string tsText;
    std::string hexText;
    if (!(stream >> tsText >> hexText)) {
        result.status  = FrameStatus::kFormatError;
        result.message = "空行或字段不足";
        return result;
    }
    try {
        result.frame.timestampMs = std::stod(tsText);
    } catch (const std::exception&) {
        result.status  = FrameStatus::kFormatError;
        result.message = "时间戳无法解析: " + tsText;
        return result;
    }

    // 2) 十六进制转字节流
    std::vector<std::uint8_t> bytes = ParseHex(hexText);
    if (bytes.size() < kHeaderBytes + 1 + kCrcBytes) {
        result.status  = FrameStatus::kFormatError;
        result.message = "帧长不足: " + std::to_string(bytes.size()) + " 字节";
        return result;
    }

    // 3) 校验帧头
    if (bytes[0] != kSofHigh || bytes[1] != kSofLow) {
        result.status  = FrameStatus::kFormatError;
        result.message = "帧头不是 AA55";
        return result;
    }

    // 4) 校验长度字段与实际载荷是否自洽
    std::size_t payloadLen = bytes[kLenIndex];
    std::size_t expectLen  = kHeaderBytes + 1 + payloadLen + kCrcBytes;
    if (bytes.size() != expectLen) {
        result.status  = FrameStatus::kFormatError;
        result.message = "长度字段 " + std::to_string(payloadLen) +
                         " 与实际字节数 " + std::to_string(bytes.size()) + " 不匹配";
        return result;
    }

    // 5) CRC 校验：范围是 TYPE ~ PAYLOAD
    std::size_t crcStart = expectLen - kCrcBytes;
    std::uint16_t received = static_cast<std::uint16_t>(bytes[crcStart]) |
                             static_cast<std::uint16_t>(bytes[crcStart + 1] << 8);
    std::uint16_t computed = Crc16Ccitt(bytes.data() + 2, crcStart - 2);
    if (received != computed) {
        result.status  = FrameStatus::kCrcError;
        result.message = "CRC 不匹配: 收到 " + std::to_string(received) +
                         "，算出 " + std::to_string(computed);
        result.frame.type = bytes[2];
        result.frame.seq  = bytes[3];
        return result;
    }

    // 6) 填充字段
    result.frame.type = bytes[2];
    result.frame.seq  = bytes[3];
    result.frame.rssi = static_cast<std::int8_t>(bytes[4]);
    result.frame.payload.assign(bytes.begin() + kPayloadPos, bytes.begin() + crcStart);
    result.status = FrameStatus::kOk;
    return result;
}

}  // namespace wlan
