#include <stdio.h>
#include <stdlib.h>
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

std::string g_inputPath;
std::vector<TestData> g_testDatas;
std::atomic<int> g_index;
std::atomic<int> g_passCount;
std::atomic<int> g_skipCount;
std::string g_skipPattern;

int main(int argc, char* argv[])
{
    std::string shellPath = "escargot";
    if (argc >= 2) {
        shellPath = argv[1];
    }
    int numThread = std::thread::hardware_concurrency();
    if (argc >= 3) {
        numThread = std::stoi(argv[2]);
    }
    if (argc >= 4) {
        g_skipPattern = argv[3];
    }

    g_inputPath = "test262_data";
    std::ifstream input(g_inputPath + "/data");

    std::string canBlockIsFalse;
    std::string isModule;
    std::string fullPath;
    std::string driverFile;
    std::string testFile;
    std::string code;

    int caseNum = 0;
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
        printf("%d -> %d\n", threadData[i].first, threadData[i].second);
        threads.push_back(std::thread([](std::pair<int, int> data, std::string shellPath) {
            for (int j = data.first; j < data.second; j ++) {
                std::string commandline = "TZ=US/Pacific " + shellPath;
                const auto& data = g_testDatas[j];

                if (g_skipPattern.size() && data.fullPath.find(g_skipPattern) != std::string::npos) {
                    g_skipCount++;
                    printf("SKIP [%d] %s\n", g_index++, data.fullPath.data());
                    continue;
                }

                commandline += " " + data.driverFile;
                if (data.canBlockIsFalse.size()) {
                    commandline += " --canblock-is-false";
                }

                if (data.isModule.size()) {
                    commandline += " --module";
                    commandline += " --filename-as=" + g_inputPath + data.fullPath;
                }

                commandline += " " + data.testFile;

                int result = WEXITSTATUS(std::system(commandline.data()));

                if (data.code == std::to_string(result)) {
                    g_passCount++;
                    printf("Success [%d] %s => %d\n", g_index++, data.fullPath.data(), result);
                } else {
                    printf("Fail [%d] %s,%s\n", g_index++, data.fullPath.data(), commandline.data());
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
