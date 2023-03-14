![OpenCyphal](cetlvast/suites/docs/images/html/cetl_logo.svg#gh-light-mode-only) \
![OpenCyphal](cetlvast/suites/docs/images/html/cetl_logo_dark.svg#gh-dark-mode-only) \
Cyphal Embedded Template Library
===================

> We like to pronounce CETL as "settle"

[![Forum](https://img.shields.io/discourse/https/forum.opencyphal.org/users.svg)](https://forum.opencyphal.org)
[![Documentation](https://img.shields.io/badge/docs-soon-grey.svg)](https://opencyphal.org/CETL/)

## include

The include directory contains the CETL headers all within a folder, "cetl". Types found under "cetl/pfXX" folders and
within `cetl::pfxx` namespaces are ["polyfill"](https://en.wikipedia.org/wiki/Polyfill_(programming)) types that adhere
to the XX standard. For example, `cetl::pf20::span` is a type that adheres, as closely as possible, to the C++20
specification for the `std::span` type.

> Any type found directly under the cetl folder does *not* adhere to a known standard and is specific only to CETL.

Also under each "cetl/pfXX" folder will be a "cetlpf.h" header. These headers enable automatic polyfill behavior but do
so by violating certain AUTOSAR-14 rules. We recommend using the CETL polyfill types directly in code that adheres
to AUTOSAR-14.

TODO: Once [Issue #12](https://github.com/OpenCyphal-Garage/CETL/issues/12) is complete we'll provide some more
info here on how we expect you to take a dependency on CETL.

## CETLVaSt

> Staying with the theme, you can call this "settle-vast"

We recommend you build the CETL VerificAtion SuiTe using your target toolchain and run the suite on
your target hardware to ensure it is fully compatible. The suite is designed to work with minimal
platform I/O and does not require a filesystem. On thicker platforms, like linux, CETLVaSt can be
used to generate more robust output and even coverage reports to verify that the test suite is
covering the entire set of CETL types.

## Project Design Tenets

- **CETL supports C++14 and newer** – It is not a C++98 compatibility library, it is not a C++11 compatibility library,
and it reserves the right to increase the base support over the years as the C++ language evolves.
- **CETL does not supplant STL, ETL, boost, or any other full-featured C++ support library** – It is not a general-purpose
C++ support library and is limited to the minimum set of types needed to ensure OpenCyphal C++ projects are agnostic to
these larger projects and are easy to integrate with.
- **Where CETL types provide backwards compatibility, they should support direct replacement by newer concepts** – Where
CETL types provide functionality found in newer C++ standards the CETL version will prefer mimicking the standard over
mimicking other support libraries like Boost.
- **CETL types will never _require_ use of the default STL heap** – They may allow use of generalized heap memory and/or
the default STL allocator but will always support an alternative way to manage their memory.
- **CETL minimizes type aliasing and never injects typedef or macros into external namespaces.** – If an `std::uint8_t`
will suffice CETL uses that explicitly. If a function should be constexpr the constexpr keyword will be used. etc.
- **CETL tries really, really hard to not use macros** – Except where AUTOSAR-14 Rule A16-0-1 permits, CETL does
not use any C macros where a C++ template or other construct will suffice.
- **CETL is [Autosar C++14](https://www.autosar.org/fileadmin/standards/adaptive/20-11/AUTOSAR_RS_CPP14Guidelines.pdf)
compliant** – Where it violates Autosar rules comments will provide a clear rationale.
- **CETL headers have minimal dependencies** – While there is a `cetl/cetl.h` it is minimal and does not drag a large
set of conventions, typedefs, and other constructs that will pollute your code. Each type provided is isolated as much
as practical and users that want to copy and paste one of the CETL headers into their project can easily elide cetl.h
with minimal effort.


## Support Matrix

| CETL Version | C++ Base Version | C++ Target Version | Current Maturity      | Release Date | Security Fixes | EOL   |
|--------------|------------------|--------------------|-----------------------|--------------|----------------|-------|
| 1.x          | C++14            | C++20              | ![pre-alpha](https://img.shields.io/badge/status-beta-blue) | (TBA) | (TBA) | (TBA) |


The above support is not guaranteed, as this is an open source project and there are no contractual obligations
agreed to by any party that contributes; however, the OpenCyphal community will use this support matrix as guidelines
to shape their work.

**C++ Base Version** – The version of C++ required to use this release of CETL.

**C++ Target Version** – The newest version of C++ this version of CETL was tested against. This version may increase
without a new release if a new version is released and the community is able to verify the existing release against it.

**Release Date** – The date after which active development of new features for a given release will cease and all updates
will be bug fixes only.

**Security Fixes** – The date after which no bug fixes will be accepted unless they are to patch critical security
vulnerabilities (See [SECURITY.md](./SECURITY.md)).

**End-Of-Life (EOL)** – The date after which no changes of any kind will be accepted.

![OpenCyphal](cetlvast/suites/docs/images/html/opencyphal_logo_dark.svg#gh-dark-mode-only)\
![OpenCyphal](cetlvast/suites/docs/images/html/opencyphal_logo.svg#gh-light-mode-only)
