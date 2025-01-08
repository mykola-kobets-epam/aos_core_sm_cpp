/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INSTANCEIDPROVIDER_MOCK_HPP_
#define INSTANCEIDPROVIDER_MOCK_HPP_

#include <gmock/gmock.h>

#include "logprovider/logprovider.hpp"

namespace aos::sm::logprovider {

class InstanceIDProviderMock : public InstanceIDProviderItf {
public:
    MOCK_METHOD(RetWithError<std::vector<std::string>>, GetInstanceIDs, (const cloudprotocol::InstanceFilter& filter),
        (override));
};

} // namespace aos::sm::logprovider

#endif // INSTANCEIDPROVIDER_MOCK_HPP_
