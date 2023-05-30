#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <atomic>
#include <vector>
#include <thread>
#include <algorithm>

struct TestData {
    std::string canBlockIsFalse;
    std::string isModule;
    std::string fullPath;
    std::string driverFile;
    std::string testFile;
    std::string code;
};

enum TestKind {
    General,
    Test262
};

std::string g_inputPath;
std::vector<TestData> g_testDatas;
std::atomic<int> g_index;
std::atomic<int> g_passCount;
std::atomic<int> g_skipCount;
std::string g_skipPattern;
std::string g_env;
TestKind g_testKind;

std::pair<std::string, int> exec(const std::string& cmd)
{
    char buffer[256];
    std::string result;
    FILE* fp = popen((cmd + " 2>&1").data(), "r");
    if (!fp) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        result += buffer;
    }
    return std::make_pair(result, WEXITSTATUS(pclose(fp)));
}

int main(int argc, char* argv[])
{
    std::string shellPath = "escargot";
    int numThread = std::thread::hardware_concurrency();
    g_inputPath = "";
    g_testKind = TestKind::General;

    for (int i = 1; i < argc; i++) {
        if (strlen(argv[i]) >= 2 && argv[i][0] == '-') { // parse command line option
            if (argv[i][1] == '-') { // `--option` case
                if (strcmp(argv[i], "--shell") == 0) {
                    if (argc > i) {
                        shellPath = argv[++i];
                        continue;
                    }
                } else if (strcmp(argv[i], "--test-data") == 0) {
                    if (argc > i) {
                        g_inputPath = argv[++i];
                        continue;
                    }
                } else if (strcmp(argv[i], "--test") == 0) {
                    if (argc > i) {
                        std::string kind = argv[++i];
                        if (kind == "test262") {
                            g_testKind = TestKind::Test262;
                        }
                        continue;
                    }
                } else if (strcmp(argv[i], "--threads") == 0) {
                    if (argc > i) {
                        numThread = std::stoi(argv[++i]);
                        continue;
                    }
                }else if (strcmp(argv[i], "--env") == 0) {
                    if (argc > i) {
                        g_env = argv[++i];
                        continue;
                    }
                }
            } else { // `-option` case
            }
            fprintf(stderr, "Cannot recognize option `%s`", argv[i]);
            continue;
        }
    }

    if (g_inputPath == "") {
        puts("please specifiy test data option with --test-data <path>");
        return -1;
    }

    if (g_testKind == TestKind::Test262) {
        g_env = "TZ=US/Pacific " + g_env;
    }

    int caseNum = 0;
    if (g_testKind == TestKind::Test262) {
        std::ifstream input(g_inputPath + "/data");

        std::string canBlockIsFalse;
        std::string isModule;
        std::string fullPath;
        std::string driverFile;
        std::string testFile;
        std::string code;

        while (std::getline(input, canBlockIsFalse)) {
            std::getline(input, isModule);
            std::getline(input, fullPath);
            std::getline(input, driverFile);
            std::getline(input, testFile);
            std::getline(input, code);

            g_testDatas.push_back({
                canBlockIsFalse,
                isModule,
                fullPath,
                driverFile,
                testFile,
                code
            });
            caseNum++;
        }
    } else {
        std::ifstream input(g_inputPath);

        std::string commandLine;
        std::string code;
        while (std::getline(input, commandLine)) {
            std::getline(input, code);
            g_testDatas.push_back({
                "",
                "",
                "",
                "",
                commandLine,
                code
            });
            caseNum++;
        }
    }

    printf("Total case number %d\n", caseNum);

    std::vector<std::pair<int, int>> threadData;
    std::vector<std::thread> threads;
    int threadSize = caseNum / numThread;

    int d = 0;
    for (int i = 0; i < numThread; i ++) {
        threadData.push_back(std::make_pair(
            d, d + threadSize
            ));
        d += threadSize;
    }

    threadData.back().second = caseNum;

    for (int i = 0; i < numThread; i ++) {
        threads.push_back(std::thread([](std::pair<int, int> data, std::string shellPath) {
            for (int j = data.first; j < data.second; j ++) {
                std::string commandline = g_env + " " + shellPath;
                const auto& data = g_testDatas[j];

                commandline += " " + data.driverFile;
                if (data.canBlockIsFalse.size()) {
                    commandline += " --canblock-is-false";
                }

                if (data.isModule.size()) {
                    commandline += " --module";
                    commandline += " --filename-as=" + g_inputPath + data.fullPath;
                }

                commandline += " " + data.testFile;

                std::string info = data.fullPath.size() ? data.fullPath : commandline;

                if (g_skipPattern.size() && data.fullPath.find(g_skipPattern) != std::string::npos) {
                    g_skipCount++;
                    printf("SKIP [%d] %s\n", g_index++, info.data());
                    continue;
                }

                auto result = exec(commandline);

                if (data.code == std::to_string(result.second)) {
                    g_passCount++;
                    printf("Success [%d] %s => %d\n", g_index++, info.data(), result.second);
                } else {
                    printf("Fail [%d] %s\n", g_index++, commandline.data());
                    printf("Fail output->\n%s", result.first.data());
                }
            }
        }, threadData[i], shellPath));
    }

    for (int i = 0; i < numThread; i ++) {
        threads[i].join();
    }

    printf("Result -> %d/%d(Skipped %d) : ", ((int)g_passCount + (int)g_skipCount), caseNum, (int)g_skipCount);
    if ((g_passCount + g_skipCount) == caseNum) {
        puts("Passed");
    } else {
        puts("Failed");
    }

    return 0;
}
