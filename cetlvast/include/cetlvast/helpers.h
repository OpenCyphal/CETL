/// @file
/// CETL VerificAtion SuiTe – Test suite helpers.
/// @copyright Copyright (c) 2023 Amazon.com Inc. and its affiliates. All Rights Reserved.

#ifndef CETLVAST_HELPERS_H_INCLUDED
#define CETLVAST_HELPERS_H_INCLUDED

#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace cetlvast
{

/// Common test for compilation under C++20
#define IS_CPP20 (__cplusplus >= 202002L)

} // namespace cetlvast

#endif  // CETLVAST_HELPERS_H_INCLUDED
