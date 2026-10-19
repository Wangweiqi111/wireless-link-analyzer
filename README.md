# Wireless Link Analyzer｜无线链路数据帧分析与稳定性测试工具

用 C++ 写的空口抓包分析小工具：读入十六进制数据帧，自动统计**错帧率、丢包率、RSSI 分布、抖动、吞吐**，
输出链路质量报告。对应 Wi-Fi / BLE 连接调试里最常干的活——判断"这条链路稳不稳"。

---

## 一、它能干什么

输入是一份抓包日志（每行一条帧），输出是这样一份报告：

```
抓包行数        : 321 行
有效帧数        : 317 帧
CRC 错帧        : 3 帧
格式非法帧      : 1 帧
丢帧（按序列号推算）: 7 帧
--------------------------------------
错帧率          : 1.25 %
丢包率          : 2.16 %
平均帧间隔      : 21.18 ms
最大抖动        : 33.23 ms
RSSI 平均/最弱/最强 : -68.9 / -82 / -55 dBm
弱信号帧占比    : 24.6 %
有效吞吐        : 5.757 kbps
RSSI 分布（每 5dBm 一档）: 直方图
```

## 二、帧格式约定

```
AA55 | TYPE(1B) | SEQ(1B) | RSSI(1B, 有符号 dBm) | LEN(1B) | PAYLOAD(NB) | CRC16(2B, 小端)
```

- `SOF` 固定 `AA55`，用来在字节流里定位帧头
- `SEQ` 为 8 位序列号，会回绕，**统计丢包就是看它跳没跳号**
- `RSSI` 用 `int8_t` 存储，天然表示负数 dBm
- `CRC16` 采用 CRC-16/CCITT-FALSE，校验范围 TYPE ~ PAYLOAD，和常见无线协议一致

## 三、编译与运行

**方式一：直接 g++（最省事）**

```bash
g++ -std=c++17 src/*.cpp -o analyzer
./analyzer data/capture_sample.txt out/report.txt
```

**方式二：CMake**

```bash
cmake -B build
cmake --build build
./build/analyzer data/capture_sample.txt out/report.txt
```

> 没装 MinGW 也可以临时用 `zig c++ -std=c++17 src/*.cpp -o analyzer` 编译。

重新生成样例数据（需要 Python 3）：`python tools/make_capture.py`

## 四、代码结构

```
src/frame.h   frame.cpp      帧解析：十六进制转字节、CRC 校验、字段提取（纯函数层）
src/analyzer.h analyzer.cpp  统计：LinkAnalyzer 类累积状态并生成报告
src/main.cpp                 IO 与命令行参数处理
data/capture_sample.txt      样例抓包（含 3 帧 CRC 错、1 帧格式错、若干丢帧）
tools/make_capture.py        样例数据生成器
```

设计上刻意做了分层：**frame 层只管解析不知道统计，analyzer 层只管统计不管 IO**，
所以接真实的串口数据时，只要把 `main.cpp` 里的文件读取换成串口读取即可复用。

## 五、这里用到的 C++ 知识点（面试能讲的）

| 知识点 | 用在哪 |
| ------ | ------ |
| 类与对象、单一职责 | `LinkAnalyzer` 封装统计状态 |
| 头文件与实现分离 | `frame.h/.cpp`、`analyzer.h/.cpp` 分层 |
| STL 容器 | `vector` 存 payload 与帧间隔，`map` 做 RSSI 直方图（有序输出） |
| RAII | `ifstream/ofstream` 依赖析构自动关闭文件，不手动 `close()` |
| 有符号字节处理 | `int8_t` 表示 RSSI 负数 |
| 位运算 | CRC 移位异或、CRC 小端拼接 |
| const 引用传参 | `Accept(const ParseResult&)` 避免拷贝 |
| 命名空间 | `namespace wlan` 防止命名污染 |
| 异常处理 | `std::stod` 解析时间戳的异常兜底 |

## 六、一个实现细节值得留意

**丢包怎么统计**：连续两帧有效数据的 `SEQ` 应相差 1；若相差 n（n>1），说明中间丢了 n-1 帧。
SEQ 是 8 位的会回绕（255 → 0），所以 `delta < 0` 时要 `+256` 再算。
另外，被判 CRC 错误的帧不参与序列号跟踪，因此它们会以"序列号缺失"的形式体现在丢包数里——这也是真实链路里
"错帧最终都表现为丢包"的工程事实。

## 七、可写进简历的表述（参考）

> **无线链路数据帧分析与稳定性测试工具 · 独立开发**
> · 基于 C++17 实现：解析十六进制空口帧（帧头同步、长度自洽校验、CRC-16/CCITT 校验），自动统计错帧率、丢包率、RSSI 分布、抖动与吞吐，输出链路质量报告
> · 采用 frame / analyzer 两层结构：解析层与统计层解耦，替换 main 层数据源即可接入串口实采数据
> · 运用 STL（vector / map）与 RAII（ifstream/ofstream 自动管理文件句柄），处理 8 位序列号回绕与异常输入校验
