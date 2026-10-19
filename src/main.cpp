#include <fstream>
#include <iostream>
#include <string>

#include "analyzer.h"
#include "frame.h"

namespace {

void PrintUsage() {
    std::cout << "用法: analyzer <抓包文件> [报告输出文件]\n"
              << "示例: analyzer data/capture_sample.txt out/report.txt\n"
              << "若省略参数，默认读取 data/capture_sample.txt\n";
}

// RAII：依赖 ifstream 析构自动关闭文件，无需手动 close
bool RunAnalyzer(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream input(inputPath);
    if (!input.is_open()) {
        std::cerr << "打不开输入文件: " << inputPath << "\n";
        return false;
    }

    wlan::LinkAnalyzer analyzer;
    std::string line;
    long lineNo = 0;
    while (std::getline(input, line)) {
        ++lineNo;
        if (line.empty()) continue;

        wlan::ParseResult result = wlan::ParseLine(line);
        analyzer.Accept(result);

        // 前 5 条异常帧打印详细原因，方便快速定位问题
        if (result.status != wlan::FrameStatus::kOk && lineNo <= 40) {
            std::cout << "[第 " << lineNo << " 行] "
                      << (result.status == wlan::FrameStatus::kCrcError ? "CRC 错误" : "格式错误")
                      << " -> " << result.message << "\n";
        }
    }

    std::cout << "\n";
    analyzer.PrintReport(std::cout);

    std::ofstream output(outputPath);
    if (output.is_open()) {
        analyzer.PrintReport(output);
        std::cout << "\n报告已写入: " << outputPath << "\n";
    } else {
        std::cerr << "警告: 无法写入报告文件 " << outputPath << "\n";
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string inputPath  = "data/capture_sample.txt";
    std::string outputPath = "out/report.txt";

    if (argc >= 2) inputPath = argv[1];
    if (argc >= 3) outputPath = argv[2];

    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        PrintUsage();
        return 0;
    }

    return RunAnalyzer(inputPath, outputPath) ? 0 : 1;
}
