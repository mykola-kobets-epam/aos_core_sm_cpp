/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LOGRECEIVER_MOCK_HPP_
#define LOGRECEIVER_MOCK_HPP_

#include <gmock/gmock.h>

#include "logprovider/logprovider.hpp"

namespace aos::sm::logprovider {

class LogReceiverMock : public LogObserverItf {
public:
    MOCK_METHOD(Error, OnLogReceived, (const cloudprotocol::PushLog& log), (override));
};

} // namespace aos::sm::logprovider

#endif // LOGRECEIVER_MOCK_HPP_
