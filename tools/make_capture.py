#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成一份模拟的空口抓包数据 feeds/data/capture_sample.txt

每行格式：<时间戳ms> <十六进制帧>
帧结构：AA55 | TYPE(1B) | SEQ(1B) | RSSI(1B, 有符号) | LEN(1B) | PAYLOAD(NB) | CRC16(2B, 小端)
人为注入：3 帧 CRC 错误、1 帧格式错误、若干丢帧（SEQ 跳号），用来验证统计逻辑。
"""

import random
import os

SOF = bytes([0xAA, 0x55])


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def make_frame(seq: int, rssi: int, payload_len: int, corrupt: bool = False) -> bytes:
    frame_type = 0x10
    payload = bytes(random.randrange(0, 256) for _ in range(payload_len))
    body = bytes([frame_type, seq, rssi & 0xFF, payload_len]) + payload
    crc = crc16_ccitt(body)
    if corrupt:
        crc ^= 0x00FF
    return SOF + body + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def main() -> None:
    random.seed(20261019)
    here = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(here, "..", "data", "capture_sample.txt")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    lines = []
    ts = 1000.0
    seq = 0
    for i in range(320):
        # 模拟 20ms 上报周期 + 随机抖动
        ts += 20.0 + random.uniform(-6.0, 8.0)
        # 信号强度整体在 -55 ~ -82 dBm 之间波动
        rssi = random.randint(-82, -55)
        payload_len = random.choice([8, 16, 24])

        # 注入 3 帧 CRC 错误
        corrupt = i in (37, 118, 245)
        # 丢帧：让 SEQ 跳过，模拟空中丢包
        skip = 2 if i in (64, 199) else 0

        seq = (seq + 1 + skip) & 0xFF
        hex_text = make_frame(seq, rssi, payload_len, corrupt).hex().upper()
        lines.append(f"{ts:.3f} {hex_text}")

    # 注入 1 行格式错误（少一个字节）
    lines.insert(150, f"{ts + 20:.3f} AA55100AC83")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print(f"已生成抓包样例: {os.path.normpath(out_path)}，共 {len(lines)} 行")


if __name__ == "__main__":
    main()
