/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <filesystem>
#include <gmock/gmock.h>

#include <aos/test/log.hpp>

#include "instanceidprovidermock.hpp"
#include "logprovider/logprovider.hpp"
#include "runner/runner.hpp"

#include <Poco/InflatingStream.h>
#include <Poco/StreamCopier.h>
#include <Poco/UTF8String.h>

using namespace testing;

namespace aos::sm {

using namespace aos::sm::logprovider;
using namespace aos::sm::runner;

namespace {

std::filesystem::path GetRuntimeDir()
{
    const auto testDir = std::filesystem::canonical("/proc/self/exe").parent_path();

    return testDir / "runtime";
}

std::string UnzipData(const String& compressedData)
{
    std::stringstream outputStream;
    std::stringstream inputStream;

    Poco::InflatingOutputStream inflater(outputStream, Poco::InflatingStreamBuf::STREAM_GZIP);

    inputStream.write(compressedData.begin(), compressedData.Size());

    Poco::StreamCopier::copyStream(inputStream, inflater);
    inflater.close();

    return outputStream.str();
}

class RunStatusReceiverStub : public RunStatusReceiverItf {
public:
    Error UpdateRunStatus(const Array<RunStatus>& units) override
    {
        LOG_INF() << "OnUnitChanged: units num=" << units.Size();

        for (const auto& unit : units) {
            LOG_INF() << "OnUnitChanged: id=" << unit.mInstanceID << ", state=" << unit.mState.ToString()
                      << ", err=" << unit.mError;
        }

        return ErrorEnum::eNone;
    }
};

class LogObserver : public LogObserverItf {
public:
    Error OnLogReceived(const cloudprotocol::PushLog& log) override
    {
        LOG_INF() << "PushLog: type: " << log.mMessageType.ToString() << ", nodeID: " << log.mNodeID
                  << ", logID: " << log.mLogID << ", part: " << log.mPart << " / " << log.mPartsCount
                  << ", status: " << log.mStatus.ToString() << ", errorInfo: " << log.mErrorInfo;

        const auto content = UnzipData(log.mContent);

        LOG_INF() << "compressed-content-size: " << log.mContent.Size() << ", content-size: " << content.size()
                  << ", content: " << Poco::UTF8::escape(content).c_str();

        return ErrorEnum::eNone;
    }
};

void CopyHelloWorldServiceFiles()
{
    const auto svcPath    = std::filesystem::canonical(__FILE__).parent_path() / "aos-service@.service";
    const auto scriptPath = std::filesystem::canonical(__FILE__).parent_path() / "hello-world.sh";

    std::filesystem::copy_file(
        svcPath, "/lib/systemd/system/aos-service@.service", std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(scriptPath, "/opt/hello-world.sh", std::filesystem::copy_options::overwrite_existing);
}

void RemoveHelloWorldServiceFiles()
{
    if (std::filesystem::exists("/lib/systemd/system/aos-service@.service")) {
        std::filesystem::remove_all("/lib/systemd/system/aos-service@.service");
    }

    if (std::filesystem::exists("/opt/hello-world.sh")) {
        std::filesystem::remove_all("/opt/hello-world.sh");
    }
}

void CopyHelloWorldCrashServiceFiles()
{
    const auto svcPath    = std::filesystem::canonical(__FILE__).parent_path() / "aos-service@test-crash.service";
    const auto scriptPath = std::filesystem::canonical(__FILE__).parent_path() / "hello-world.sh";

    std::filesystem::copy_file(
        svcPath, "/lib/systemd/system/aos-service@.service", std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(scriptPath, "/opt/hello-world.sh", std::filesystem::copy_options::overwrite_existing);
}

void RemoveHelloWorldCrashServiceFiles()
{
    if (std::filesystem::exists("/lib/systemd/system/aos-service@.service")) {
        std::filesystem::remove_all("/lib/systemd/system/aos-service@.service");
    }

    if (std::filesystem::exists("/opt/hello-world.sh")) {
        std::filesystem::remove_all("/opt/hello-world.sh");
    }
}

} // namespace

/**
 * A small integration test of runner & logprovider.
 */
class IntegrTest : public Test {
public:
    void SetUp() override
    {
        test::InitLog();

        if (!std::filesystem::exists(GetRuntimeDir())) {
            std::filesystem::create_directories(GetRuntimeDir());
        }

        ASSERT_TRUE(mRunner.Init(mRunStatusReceiver).IsNone());
        ASSERT_TRUE(mRunner.Start().IsNone());

        config::LoggingConfig loggingConfig = {200U, 10U};

        ASSERT_TRUE(mLogProvider.Init(loggingConfig, mInstanceIDProvider).IsNone());
        ASSERT_TRUE(mLogProvider.Start().IsNone());

        ASSERT_TRUE(mLogProvider.Subscribe(mLogObserver).IsNone());
    }

    void TearDown() override
    {
        ASSERT_TRUE(mLogProvider.Unsubscribe(mLogObserver).IsNone());

        ASSERT_TRUE(mRunner.Stop().IsNone());
        ASSERT_TRUE(mLogProvider.Stop().IsNone());

        if (std::filesystem::exists(GetRuntimeDir())) {
            std::filesystem::remove_all(GetRuntimeDir());
        }
    }

protected:
    const std::filesystem::path cRuntimeDir = GetRuntimeDir();

    InstanceIDProviderMock mInstanceIDProvider;
    RunStatusReceiverStub  mRunStatusReceiver;
    Runner                 mRunner;

    LogObserver mLogObserver;
    LogProvider mLogProvider;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(IntegrTest, DISABLED_GetServiceInstance)
{
    CopyHelloWorldServiceFiles();

    auto now = Time::Now();

    // Start service instance

    LOG_INF() << "\n\n============================";

    RunParameters params   = {};
    auto          instance = mRunner.StartInstance("service0", cRuntimeDir.c_str(), params);

    LOG_INF() << "Starting service with ID=" << instance.mInstanceID << ", state=" << instance.mState.ToString()
              << ", err=" << instance.mError << " ...";

    const auto expectedRes = RunStatus {"service0", InstanceRunStateEnum::eActive, Error()};
    ASSERT_EQ(instance, expectedRes);
    sleep(12); // wait for unit update

    // Send GetInstanceLog request
    LOG_INF() << "\n\n============================";
    LOG_INF() << "Get service instance log...";

    std::vector<std::string> instanceIDs = {"service0"};
    EXPECT_CALL(mInstanceIDProvider, GetInstanceIDs(_))
        .WillOnce(Return(RetWithError<std::vector<std::string>>(instanceIDs, ErrorEnum::eNone)));

    cloudprotocol::LogFilter logFilter = {};

    logFilter.mFrom.SetValue(now);
    logFilter.mNodeIDs.PushBack("node0");
    logFilter.mInstanceFilter.mServiceID.SetValue("hello-world-service");

    cloudprotocol::RequestLog request;

    request.mFilter      = logFilter;
    request.mLogID       = "log0";
    request.mLogType     = cloudprotocol::LogTypeEnum::eServiceLog;
    request.mMessageType = cloudprotocol::LogMessageTypeEnum::eRequestLog;

    EXPECT_TRUE(mLogProvider.GetInstanceLog(request).IsNone());

    sleep(2); // wait for listener to receive PushLog updates

    // Stop service instance
    LOG_INF() << "\n\n============================";
    LOG_INF() << "Stop service instance...";

    EXPECT_TRUE(mRunner.StopInstance("service0").IsNone());
    sleep(2); // wait for unit update

    RemoveHelloWorldServiceFiles();
}

TEST_F(IntegrTest, DISABLED_GetInstanceCrashLog)
{
    CopyHelloWorldCrashServiceFiles();

    auto now = Time::Now();

    // Start service instance

    LOG_INF() << "\n\n============================";

    RunParameters params   = {};
    auto          instance = mRunner.StartInstance("service1", cRuntimeDir.c_str(), params);

    LOG_INF() << "Starting service with ID=" << instance.mInstanceID << ", state=" << instance.mState.ToString()
              << ", err=" << instance.mError << " ...";

    const auto expectedRes = RunStatus {"service1", InstanceRunStateEnum::eActive, Error()};
    ASSERT_EQ(instance, expectedRes);

    sleep(5); // wait for unit update

    // Send GetInstanceLog request
    LOG_INF() << "\n\n============================";
    LOG_INF() << "Get service instance crash log...";

    std::vector<std::string> instanceIDs = {"service1"};
    EXPECT_CALL(mInstanceIDProvider, GetInstanceIDs(_))
        .WillOnce(Return(RetWithError<std::vector<std::string>>(instanceIDs, ErrorEnum::eNone)));

    cloudprotocol::LogFilter logFilter = {};

    logFilter.mFrom.SetValue(now);
    logFilter.mNodeIDs.PushBack("node0");
    logFilter.mInstanceFilter.mServiceID.SetValue("hello-world-service");

    cloudprotocol::RequestLog request;

    request.mFilter      = logFilter;
    request.mLogID       = "log0";
    request.mLogType     = cloudprotocol::LogTypeEnum::eServiceLog;
    request.mMessageType = cloudprotocol::LogMessageTypeEnum::eRequestLog;

    EXPECT_TRUE(mLogProvider.GetInstanceCrashLog(request).IsNone());

    sleep(4); // wait for listener to receive PushLog updates

    // Stop service instance
    // LOG_INF() << "\n\n============================";
    // LOG_INF() << "Stop service instance...";

    // //EXPECT_TRUE(mRunner.StopInstance("service1").IsNone());
    // //sleep(2); // wait for unit update

    RemoveHelloWorldCrashServiceFiles();
}

TEST_F(IntegrTest, DISABLED_GetSystemLog)
{
    auto now = Time::Now();
    sleep(4);

    // Send get log request
    LOG_INF() << "\n\n============================";
    LOG_INF() << "Get system log...";

    cloudprotocol::LogFilter logFilter = {};

    logFilter.mFrom.SetValue(now);
    logFilter.mNodeIDs.PushBack("node0");
    logFilter.mInstanceFilter.mServiceID.SetValue("hello-world-service");

    cloudprotocol::RequestLog request;

    request.mFilter      = logFilter;
    request.mLogID       = "log0";
    request.mLogType     = cloudprotocol::LogTypeEnum::eSystemLog;
    request.mMessageType = cloudprotocol::LogMessageTypeEnum::eRequestLog;

    EXPECT_TRUE(mLogProvider.GetSystemLog(request).IsNone());

    sleep(4); // wait for listener to receive PushLog updates
}

} // namespace aos::sm
