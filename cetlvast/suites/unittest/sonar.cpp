/// @file
/// Hack to get sonarqube to analyze cetl headers.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetl/rtti.hpp"
#include "cetl/type_traits_ext.hpp"
#include "cetl/unbounded_variant.hpp"
#include "cetl/variable_length_array.hpp"
#include "cetl/visit_helpers.hpp"
#include "cetl/pmr/buffer_memory_resource_delegate.hpp"
#include "cetl/pmr/function.hpp"
#include "cetl/pmr/interface_ptr.hpp"
#include "cetl/pmr/memory.hpp"
#include "cetl/pf17/array_memory_resource.hpp"
#include "cetl/pf17/buffer_memory_resource.hpp"
#include "cetl/pf17/byte.hpp"
#include "cetl/pf17/memory_resource.hpp"
#include "cetl/pf17/optional.hpp"
#include "cetl/pf17/string_view.hpp"
#include "cetl/pf17/type_traits.hpp"
#include "cetl/pf17/utility.hpp"
#include "cetl/pf17/variant.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"
#include "cetl/pf20/span.hpp"

int main()
{
    return 0;
}
