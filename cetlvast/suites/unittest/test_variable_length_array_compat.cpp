/// @file
/// Unit tests that confirm cetl::VariableLengthArray behaviour versus std::vector.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words wself

#include "cetl/variable_length_array.hpp"

#include "cetl/pf17/sys/memory_resource.hpp"
#include "cetl/pmr/buffer_memory_resource_delegate.hpp"
#include "cetl/pf17/byte.hpp"

#include "cetlvast/helpers_gtest.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"

#include <memory>
#include <type_traits>
#include <limits>
#include <string>
#include <array>
#include <stdexcept>
#include <vector>
#if (__cplusplus >= CETL_CPP_STANDARD_17)
#    include <memory_resource>
#endif

// +-------------------------------------------------------------------------------------------------------------------+
// | standard allocator with max_size defined.
// +-------------------------------------------------------------------------------------------------------------------+

template <typename T, std::size_t max_size_v>
struct MaxAllocator : public std::allocator<T>
{
    template <typename U>
    struct rebind
    {
        typedef MaxAllocator<U, max_size_v> other;
    };

    typename std::allocator<T>::size_type max_size() const noexcept
    {
        return max_size_v;
    }
};

// +-------------------------------------------------------------------------------------------------------------------+
// | memory_resource FACTORIES
// +-------------------------------------------------------------------------------------------------------------------+

/// Provides null_memory_resource
struct NullResourceFactory
{
    template <typename Allocator, typename UpstreamResourceFactoryType, typename Enable = void>
    struct Bind
    {};

    template <typename Allocator>
    struct Bind<Allocator,
                NullResourceFactory,
                typename std::enable_if<std::is_constructible<
                    Allocator,
                    typename std::add_pointer<cetl::pf17::pmr::memory_resource>::type>::value>::type>
    {
        static constexpr std::size_t expected_max_size() noexcept
        {
            return 0;
        }

        static constexpr typename std::add_pointer<cetl::pf17::pmr::memory_resource>::type resource(
            NullResourceFactory& resource_factory)
        {
            (void) resource_factory;
            return cetl::pf17::pmr::null_memory_resource();
        }

        static constexpr typename std::add_pointer<cetl::pf17::pmr::memory_resource>::type resource(
            NullResourceFactory& resource_factory,
            NullResourceFactory& upstream)
        {
            (void) upstream;
            return resource(resource_factory);
        }

        static constexpr Allocator make_allocator(NullResourceFactory& resource_factory, NullResourceFactory& upstream)
        {
            return Allocator{resource(resource_factory, upstream)};
        }
    };

    template <typename Allocator>
    struct Bind<Allocator,
                NullResourceFactory,
                typename std::enable_if<
                    std::is_base_of<std::allocator<typename Allocator::value_type>, Allocator>::value>::type>
    {
        static constexpr Allocator make_allocator(NullResourceFactory& resource_factory, NullResourceFactory& upstream)
        {
            (void) resource_factory;
            (void) upstream;
            return Allocator{};
        }
    };
};

// +-------------------------------------------------------------------------------------------------------------------+

// Creates a standard allocator that implements max_size() and returns the given MaxSizeValue.
template <std::size_t MaxSizeValue>
struct MaxSizeResourceFactory
{
    template <typename Allocator, typename UpstreamResourceFactoryType, typename Enable = void>
    struct Bind
    {};

    template <typename Allocator, typename UpstreamResourceFactoryType>
    struct Bind<Allocator,
                UpstreamResourceFactoryType,
                typename std::enable_if<std::is_constructible<
                    Allocator,
                    typename std::add_pointer<cetl::pf17::pmr::memory_resource>::type>::value>::type>
    {
        static constexpr std::size_t expected_max_size() noexcept
        {
            return MaxSizeValue;
        }

        static constexpr typename std::add_pointer<cetl::pf17::pmr::memory_resource>::type resource(
            MaxSizeResourceFactory<MaxSizeValue>& resource_factory)
        {
            (void) resource_factory;
            return cetl::pf17::pmr::null_memory_resource();
        }

        static constexpr typename std::add_pointer<cetl::pf17::pmr::memory_resource>::type resource(
            MaxSizeResourceFactory<MaxSizeValue>& resource_factory,
            NullResourceFactory&                  upstream)
        {
            (void) upstream;
            return resource(resource_factory);
        }

        static constexpr Allocator make_allocator(MaxSizeResourceFactory&      resource_factory,
                                                  UpstreamResourceFactoryType& upstream)
        {
            return Allocator{resource(resource_factory, upstream)};
        }
    };

    template <typename Allocator>
    struct Bind<Allocator,
                NullResourceFactory,
                typename std::enable_if<
                    std::is_base_of<std::allocator<typename Allocator::value_type>, Allocator>::value>::type>
    {
        static constexpr std::size_t expected_max_size() noexcept
        {
            return MaxSizeValue;
        }

        static constexpr Allocator make_allocator(MaxSizeResourceFactory& resource_factory,
                                                  NullResourceFactory&    upstream)
        {
            (void) resource_factory;
            (void) upstream;
            return Allocator{};
        }
    };
};

// +-------------------------------------------------------------------------------------------------------------------+

/// Creates a polymorphic allocator that uses the new_delete_resource.
struct CetlNewDeleteResourceFactory
{
    template <typename Allocator, typename UpstreamResourceFactoryType>
    struct Bind
    {};

    template <typename Allocator>
    struct Bind<Allocator, NullResourceFactory>
    {
        static constexpr std::size_t expected_max_size() noexcept
        {
            return std::numeric_limits<std::size_t>::max() / sizeof(typename Allocator::value_type);
        }

        static constexpr typename std::add_pointer<cetl::pf17::pmr::memory_resource>::type resource(
            CetlNewDeleteResourceFactory& resource_factory,
            NullResourceFactory&          upstream)
        {
            (void) upstream;
            (void) resource_factory;
            return cetl::pf17::pmr::new_delete_resource();
        }

        static constexpr Allocator make_allocator(CetlNewDeleteResourceFactory& resource_factory,
                                                  NullResourceFactory&          upstream)
        {
            return Allocator{resource(resource_factory, upstream)};
        }
    };
};

// +-------------------------------------------------------------------------------------------------------------------+

/// Creates a polymorphic allocator that uses a new_delete_resource that does not implement realloc.
struct CetlNoReallocNewDeleteResourceFactory
{
    template <typename Allocator, typename UpstreamResourceFactoryType>
    struct Bind
    {};

    template <typename Allocator>
    struct Bind<Allocator, NullResourceFactory>
    {
        static constexpr std::size_t expected_max_size() noexcept
        {
            return std::numeric_limits<std::size_t>::max() / sizeof(typename Allocator::value_type);
        }

        static constexpr typename std::add_pointer<cetl::pf17::pmr::memory_resource>::type resource(
            CetlNoReallocNewDeleteResourceFactory& resource_factory,
            NullResourceFactory&                   upstream)
        {
            (void) upstream;
            (void) resource_factory;
            return &resource_factory.memory_resoure_;
        }

        static constexpr Allocator make_allocator(CetlNoReallocNewDeleteResourceFactory& resource_factory,
                                                  NullResourceFactory&                   upstream)
        {
            return Allocator{resource(resource_factory, upstream)};
        }
    };

private:
    cetlvast::MaxAlignNewDeleteResourceWithoutRealloc memory_resoure_{};
};

// +-------------------------------------------------------------------------------------------------------------------+

/// Creates a polymorphic allocator that uses an UnsynchronizedBufferMemoryResourceDelegate-based memory resource.
template <std::size_t ArraySizeBytes = 24>
class CetlUnsynchronizedArrayMemoryResourceFactory
{
private:
    template <std::size_t ImplArraySizeBytes>
    class ResourceImpl : public cetl::pf17::pmr::memory_resource
    {
    public:
        explicit ResourceImpl(cetl::pf17::pmr::memory_resource* upstream)
            : memory_{}
            , delegate_{memory_.data(),
                        memory_.size(),
                        upstream,
                        cetl::pf17::pmr::deviant::memory_resource_traits<cetl::pf17::pmr::memory_resource>::max_size(
                            *upstream)}
        {
        }

    private:
        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            return delegate_.allocate(bytes, alignment);
        }

        void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
        {
            delegate_.deallocate(p, bytes, alignment);
        }

        bool do_is_equal(const cetl::pf17::pmr::memory_resource& other) const noexcept override
        {
            return (this == &other);
        }

        std::size_t do_max_size() const noexcept override
        {
            return delegate_.max_size();
        }

        void* do_reallocate(void*       p,
                            std::size_t old_size_bytes,
                            std::size_t new_size_bytes,
                            std::size_t alignment) override
        {
            return delegate_.reallocate(p, old_size_bytes, new_size_bytes, alignment);
        }

        std::array<cetl::pf17::byte, ImplArraySizeBytes>                                        memory_;
        cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource> delegate_;
    };

public:
    template <typename Allocator, typename UpstreamResourceFactoryType>
    struct Bind
    {
        static constexpr std::size_t expected_max_size() noexcept
        {
            return ArraySizeBytes / sizeof(typename Allocator::value_type);
        }

        static constexpr typename std::add_pointer<cetl::pf17::pmr::memory_resource>::type resource(
            CetlUnsynchronizedArrayMemoryResourceFactory& resource_factory,
            UpstreamResourceFactoryType&                  upstream)
        {
            auto resource = std::make_unique<ResourceImpl<ArraySizeBytes>>(
                UpstreamResourceFactoryType::template Bind<Allocator, UpstreamResourceFactoryType>::resource(upstream));
            resource_factory.resources_.push_back(std::move(resource));
            return resource_factory.resources_.back().get();
        }
        static constexpr Allocator make_allocator(CetlUnsynchronizedArrayMemoryResourceFactory& resource_factory,
                                                  UpstreamResourceFactoryType&                  upstream)
        {
            return Allocator{resource(resource_factory, upstream)};
        }
    };

private:
    std::vector<std::unique_ptr<ResourceImpl<ArraySizeBytes>>> resources_;
};

// +-------------------------------------------------------------------------------------------------------------------+

#if (__cplusplus >= CETL_CPP_STANDARD_17)

/// Creates a polymorphic allocator that uses std::pmr::new_delete_resource.
struct StdNewDeleteResourceFactory
{
    template <typename Allocator, typename UpstreamResourceFactoryType>
    struct Bind
    {};

    template <typename Allocator>
    struct Bind<Allocator, NullResourceFactory>
    {
        static constexpr std::size_t expected_max_size() noexcept
        {
            return std::numeric_limits<std::size_t>::max() / sizeof(typename Allocator::value_type);
        }

        static constexpr std::add_pointer<std::pmr::memory_resource>::type resource(
            StdNewDeleteResourceFactory& resource_factory,
            NullResourceFactory&         upstream)
        {
            (void) upstream;
            (void) resource_factory;
            return std::pmr::new_delete_resource();
        }

        static constexpr Allocator make_allocator(StdNewDeleteResourceFactory& resource_factory,
                                                  NullResourceFactory&         upstream)
        {
            return Allocator{resource(resource_factory, upstream)};
        }
    };
};
#endif

// +-------------------------------------------------------------------------------------------------------------------+
// | TEST VALUE TYPES
// +-------------------------------------------------------------------------------------------------------------------+

/// A type that is not trivially constructable for use in test subject containers.
class NotTriviallyConstructable
{
public:
    NotTriviallyConstructable()
        : data_{}
    {
    }

    NotTriviallyConstructable(std::size_t value)
        : data_{std::to_string(value)}
    {
    }

    virtual ~NotTriviallyConstructable() = default;

    NotTriviallyConstructable(const NotTriviallyConstructable& rhs)
        : data_{rhs.data_}
    {
    }

    NotTriviallyConstructable(NotTriviallyConstructable&& rhs)
        : data_{std::move(rhs.data_)}
    {
    }

    NotTriviallyConstructable& operator=(const NotTriviallyConstructable& rhs)
    {
        data_ = rhs.data_;
        return *this;
    }

    NotTriviallyConstructable& operator=(NotTriviallyConstructable&& rhs)
    {
        data_ = std::move(rhs.data_);
        return *this;
    }

    virtual const char* what() const noexcept
    {
        return data_.c_str();
    }

    operator std::size_t() const noexcept
    {
        if (data_.empty())
        {
            return 0;
        }
        else
        {
            return std::stoul(data_);
        }
    }

    NotTriviallyConstructable& operator=(std::size_t value)
    {
        data_ = std::to_string(value);
        return *this;
    }

private:
    std::string data_;
};

static_assert(!std::is_trivially_constructible<NotTriviallyConstructable>::value,
              "NotTriviallyConstructable must not be trivially constructable");

// +-------------------------------------------------------------------------------------------------------------------+
// | TYPED TEST PROTOCOL
// +-------------------------------------------------------------------------------------------------------------------+

/// The primary test protocol. Each test case will have a single VLA (the subject) with the given value_type and
/// allocator_type. The allocator_type will be constructed with the given memory_resource_factory_type and that
/// memory resource will be given the providing memory_resource_upstream_factory_type.
template <typename ContainerType,
          typename Allocator,
          typename MemoryResourceFactoryType,
          typename UpstreamMemoryResourceFactoryType = NullResourceFactory>
struct TestAllocatorType
{
    using container_type                        = ContainerType;
    using allocator_type                        = Allocator;
    using value_type                            = typename Allocator::value_type;
    using memory_resource_factory_type          = MemoryResourceFactoryType;
    using memory_resource_upstream_factory_type = UpstreamMemoryResourceFactoryType;
};

// +-------------------------------------------------------------------------------------------------------------------+
// | TEST SUITE
// +-------------------------------------------------------------------------------------------------------------------+

///
/// Test suite for running multiple allocators against the variable length array type.
///
template <typename T>
class VLATestsGeneric : public ::testing::Test
{
public:
    // Some tests will run std::min(maximumMaxSize, get_expected_max_size()) to avoid
    // trying to run std::numeric_limits<std::size_t>::max() loops or allocate that much memory.
    constexpr static std::size_t maximumMaxSize = 1024;
    using MemoryResourceFactoryType             = typename T::memory_resource_factory_type;
    using MemoryResourceFactoryTypePtr          = typename std::add_pointer<MemoryResourceFactoryType>::type;
    using MemoryResourceUpstreamFactoryType     = typename T::memory_resource_upstream_factory_type;
    using MemoryResourceUpstreamFactoryTypePtr  = typename std::add_pointer<MemoryResourceUpstreamFactoryType>::type;
    using Allocator                             = typename T::allocator_type;
    using Value                                 = typename T::value_type;
    using SubjectType = typename std::conditional<std::is_same<typename T::container_type, cetlvast::CETLTag>::value,
                                                  cetl::VariableLengthArray<Value, Allocator>,
                                                  std::vector<Value, Allocator>>::type;

    static void SetUpTestSuite()
    {
#ifdef CETLVAST_RTTI_ENABLED
        ::testing::Test::RecordProperty("TestAllocatorType", typeid(T).name());
#else
        ::testing::Test::RecordProperty("TestAllocatorType", "(RTTI disabled)");
#endif
    }
    void SetUp() override
    {
        memoryResourceUpstreamFactory_ = std::make_unique<MemoryResourceUpstreamFactoryType>();
        memoryResourceFactory_         = std::make_unique<MemoryResourceFactoryType>();
        cetlvast::InstrumentedAllocatorStatistics::reset();
    }

    // Tears down the test fixture.
    void TearDown() override
    {
        memoryResourceFactory_.reset();
        memoryResourceUpstreamFactory_.reset();
    }

    // Get the configured maximum number of objects for the allocator.
    std::size_t get_expected_max_size() const noexcept
    {
        // Interestingly enough, GCC and Clang disagree on the default maximum size
        // for std::vector. Clang says std::numeric_limits<std::ptrdiff_t>::max() and
        // GCC says std::numeric_limits<std::ptrdiff_t>::max() / sizeof(value_type).
        // We'll expect the larger of the two but tests should always evaluate max_size
        // as being <= the expected_max_size() to be portable.
        const std::size_t clamp = std::numeric_limits<std::ptrdiff_t>::max();
        return std::min(clamp,
                        MemoryResourceFactoryType::template Bind<Allocator, MemoryResourceUpstreamFactoryType>::
                            expected_max_size());
    }

    Allocator make_allocator()
    {
        return MemoryResourceFactoryType::template Bind<Allocator, MemoryResourceUpstreamFactoryType>::
            make_allocator(*memoryResourceFactory_, *memoryResourceUpstreamFactory_);
    }

private:
    std::unique_ptr<MemoryResourceFactoryType>         memoryResourceFactory_;
    std::unique_ptr<MemoryResourceUpstreamFactoryType> memoryResourceUpstreamFactory_;
};

template <typename T>
const std::size_t VLATestsGeneric<T>::maximumMaxSize;

// +-------------------------------------------------------------------------------------------------------------------+
// | TYPED TEST, TYPES
// |    See comments on TestAllocatorType for the "protocol" in use here.
// +-------------------------------------------------------------------------------------------------------------------+
// clang-format off

using MyTypes = ::testing::Types<
/*                          container type tag    | allocator type                                                     | primary memory resource factory                                                      */
/* 0 */   TestAllocatorType<cetlvast::CETLTag,      cetl::pf17::pmr::polymorphic_allocator<std::uint64_t>,               CetlUnsynchronizedArrayMemoryResourceFactory<24>>
/* 1 */ , TestAllocatorType<cetlvast::SkipTag,      cetl::pf17::pmr::polymorphic_allocator<std::uint64_t>,               CetlUnsynchronizedArrayMemoryResourceFactory<24>>
/* 2 */ , TestAllocatorType<cetlvast::CETLTag,      cetl::pf17::pmr::polymorphic_allocator<NotTriviallyConstructable>,   CetlUnsynchronizedArrayMemoryResourceFactory<sizeof(NotTriviallyConstructable) * 8>>
/* 3 */ , TestAllocatorType<cetlvast::SkipTag,      cetl::pf17::pmr::polymorphic_allocator<NotTriviallyConstructable>,   CetlUnsynchronizedArrayMemoryResourceFactory<sizeof(NotTriviallyConstructable) * 8>>
/* 4 */ , TestAllocatorType<cetlvast::STLTag,       MaxAllocator<std::uint32_t, 24>,                                     MaxSizeResourceFactory<24>>
/* 5 */ , TestAllocatorType<cetlvast::CETLTag,      cetl::pf17::pmr::polymorphic_allocator<std::uint64_t>,               CetlNewDeleteResourceFactory>
/* 6 */ , TestAllocatorType<cetlvast::STLTag,       cetl::pf17::pmr::polymorphic_allocator<std::uint64_t>,               CetlNewDeleteResourceFactory>
/* 7 */ , TestAllocatorType<cetlvast::CETLTag,      cetl::pf17::pmr::polymorphic_allocator<std::uint64_t>,               CetlNoReallocNewDeleteResourceFactory>

#if (__cplusplus >= CETL_CPP_STANDARD_17)
/* 8 */ , TestAllocatorType<cetlvast::CETLTag,      std::pmr::polymorphic_allocator<std::uint64_t>,                      StdNewDeleteResourceFactory>
/* 9 */ , TestAllocatorType<cetlvast::STLTag,       std::pmr::polymorphic_allocator<std::uint64_t>,                      StdNewDeleteResourceFactory>
#endif
>;
// clang-format on

TYPED_TEST_SUITE(VLATestsGeneric, MyTypes, );

// +-------------------------------------------------------------------------------------------------------------------+
// | TESTS
// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestReserve)
{
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};

    ASSERT_EQ(0U, subject.capacity());
    ASSERT_EQ(0U, subject.size());
    ASSERT_GE(this->get_expected_max_size(), subject.max_size());
    subject.reserve(1);
    ASSERT_LE(1U, subject.capacity());
    ASSERT_EQ(0U, subject.size());
    ASSERT_GE(this->get_expected_max_size(), subject.max_size());
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestPush)
{
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};
    ASSERT_EQ(0U, subject.size());

    typename decltype(subject)::value_type x = 0;

    const std::size_t clamped_max = std::min(this->get_expected_max_size(), this->maximumMaxSize);
    subject.reserve(clamped_max);
    for (std::size_t i = 0; i < clamped_max; ++i)
    {
        subject.push_back(x);

        ASSERT_EQ(i + 1, subject.size());
        ASSERT_LE(subject.size(), subject.capacity());

        ASSERT_EQ(x, subject[i]);
        x = x + 1;
    }
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestPop)
{
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};

    std::size_t clamped_max = std::min(this->get_expected_max_size(), 10UL);
    ASSERT_LE(1U, clamped_max) << "This test requires a max_size of at least 1.";
    subject.reserve(clamped_max);
    const std::size_t reserved = subject.capacity();
    ASSERT_LE(clamped_max, subject.capacity());
    subject.push_back(1);
    ASSERT_EQ(1U, subject.size());
    ASSERT_EQ(1, subject[0]);
    ASSERT_EQ(1U, subject.size());
    subject.pop_back();
    ASSERT_EQ(0U, subject.size());
    ASSERT_EQ(reserved, subject.capacity());
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestShrink)
{
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};

    std::size_t clamped_max = std::min(this->get_expected_max_size(), 10UL);
    ASSERT_LE(1U, clamped_max) << "This test requires a max_size of at least 1.";

    subject.reserve(clamped_max);
    const auto reserved = subject.capacity();
    ASSERT_LE(clamped_max, reserved);
    subject.push_back(1);
    ASSERT_EQ(1U, subject.size());
    ASSERT_EQ(1, subject[0]);
    ASSERT_EQ(1U, subject.size());
    ASSERT_EQ(reserved, subject.capacity());
    subject.shrink_to_fit();
    // shrink_to_fit implementations are not required to exactly match the size of the container, but they can't grow
    // the size.
    ASSERT_LE(subject.capacity(), clamped_max);
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestShrinkToSameSize)
{
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};

    subject.reserve(1);
    ASSERT_LE(1U, subject.capacity());
    subject.push_back(1);
    ASSERT_EQ(1U, subject.size());
    subject.shrink_to_fit();
    ASSERT_EQ(1U, subject.size());
    ASSERT_EQ(1U, subject.capacity());
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestCopyAssignment)
{
    if (std::is_same<typename TypeParam::container_type, cetlvast::SkipTag>::value)
    {
        GTEST_SKIP() << "Skipping test that requires CETL reallocation support.";
    }
    typename TestFixture::SubjectType subject0{TestFixture::make_allocator()};
    typename TestFixture::SubjectType subject1{TestFixture::make_allocator()};

    subject0.push_back(1);
    subject0.push_back(2);
    subject0.push_back(3);

    subject1 = subject0;

    ASSERT_EQ(subject0.size(), subject1.size());
    ASSERT_GE(subject0.capacity(), subject1.capacity());
    ASSERT_EQ(subject0.max_size(), subject1.max_size());
    ASSERT_EQ(subject0[0], subject1[0]);
    ASSERT_EQ(subject0[1], subject1[1]);
    ASSERT_EQ(subject0[2], subject1[2]);
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestOverMaxSize)
{
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};
    const std::size_t                 MaxSize = subject.max_size();
    if (MaxSize > TestFixture::maximumMaxSize)
    {
        GTEST_SKIP() << "The allocator under test has a max_size that is too large for this test.";
        return;
    }
    if (MaxSize == 0U)
    {
        GTEST_SKIP() << "The allocator under test does not have a maximum size.";
        return;
    }

    subject.reserve(MaxSize);

    for (std::size_t i = 1; i <= MaxSize; ++i)
    {
        subject.push_back(static_cast<typename TypeParam::value_type>(i));
        ASSERT_EQ(i, subject.size());
        ASSERT_EQ(static_cast<typename TypeParam::value_type>(i), subject[i - 1]);
    }

    ASSERT_EQ(MaxSize, subject.capacity());
#if defined(__cpp_exceptions)
    ASSERT_THROW(subject.reserve(MaxSize + 1), std::length_error);
    ASSERT_EQ(MaxSize, subject.capacity());
#endif

    ASSERT_EQ(MaxSize, subject.size());

#if defined(__cpp_exceptions)
    ASSERT_THROW(subject.push_back(0), std::length_error);
#endif
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestResize)
{
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};

    std::size_t clamped_max = std::min(this->get_expected_max_size(), 10UL);
    ASSERT_GT(this->get_expected_max_size(), 0) << "This test is only valid if get_expected_max_size() > 0";
    ASSERT_GT(clamped_max, subject.size());
    subject.resize(clamped_max);
    ASSERT_EQ(clamped_max, subject.size());

    typename TestFixture::SubjectType::value_type default_constructed_value{};
    ASSERT_EQ(subject[subject.size() - 1], default_constructed_value);
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestResizeToZero)
{
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};

    std::size_t clamped_max = std::min(this->get_expected_max_size(), 10UL);
    ASSERT_GT(this->get_expected_max_size(), 0) << "This test is only valid if get_expected_max_size() > 0";
    ASSERT_GT(clamped_max, subject.size());
    subject.resize(clamped_max);
    ASSERT_EQ(clamped_max, subject.size());
    std::size_t capacity_before = subject.capacity();

    subject.resize(0);
    ASSERT_EQ(0, subject.size());
    ASSERT_EQ(capacity_before, subject.capacity());
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestResizeWithCopy)
{
    if (std::is_same<typename TypeParam::container_type, cetlvast::SkipTag>::value)
    {
        GTEST_SKIP() << "Skipping test that requires CETL reallocation support.";
    }

    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};

    std::size_t clamped_max = std::min(this->get_expected_max_size(), 10UL);
    ASSERT_GT(this->get_expected_max_size(), 1) << "This test is only valid if get_expected_max_size() > 1";
    ASSERT_GT(clamped_max, subject.size());

    subject.push_back(1);

    const typename TestFixture::SubjectType::value_type copy_from_value{2};
    subject.resize(clamped_max, copy_from_value);
    ASSERT_EQ(clamped_max, subject.size());
    ASSERT_EQ(1, subject[0]);

    for (std::size_t i = 1; i < subject.size(); ++i)
    {
        ASSERT_EQ(subject[i], copy_from_value);
    }
}

// +-------------------------------------------------------------------------------------------------------------------+

TYPED_TEST(VLATestsGeneric, TestFrontAndBack)
{
    using const_ref_value_type =
        typename std::add_lvalue_reference<typename std::add_const<typename TypeParam::value_type>::type>::type;
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};
    subject.reserve(2);
    subject.push_back(static_cast<typename TypeParam::value_type>(1));
    subject.push_back(static_cast<typename TypeParam::value_type>(2));
    ASSERT_EQ(static_cast<typename TypeParam::value_type>(1), subject.front());
    typename TestFixture::SubjectType::const_reference const_front_ref =
        const_cast<typename std::add_pointer<typename std::add_const<typename TestFixture::SubjectType>::type>::type>(
            &subject)
            ->front();
    ASSERT_EQ(static_cast<const_ref_value_type>(1), const_front_ref);

    typename TestFixture::SubjectType::const_reference const_back_ref =
        const_cast<typename std::add_pointer<typename std::add_const<typename TestFixture::SubjectType>::type>::type>(
            &subject)
            ->back();
    ASSERT_EQ(static_cast<typename TypeParam::value_type>(2), subject.back());
    ASSERT_EQ(static_cast<const_ref_value_type>(2), const_back_ref);
}

// +----------------------------------------------------------------------+

#ifdef __cpp_exceptions

TYPED_TEST(VLATestsGeneric, TestResizeExceptionLengthError)
{
    typename TestFixture::SubjectType subject{TestFixture::make_allocator()};
    ASSERT_THROW(subject.resize(subject.max_size() + 1), std::length_error);
}

#endif  // __cpp_exceptions

// +----------------------------------------------------------------------+
/**
 * Test suite to ensure non-trivial objects are properly handled. This one is both for bool and non-bool spec.
 */
template <typename T>
class VLATestsNonTrivialCommon : public ::testing::Test
{};
using VLATestsNonTrivialCommonTypes = ::testing::Types<int, bool>;
TYPED_TEST_SUITE(VLATestsNonTrivialCommon, VLATestsNonTrivialCommonTypes, );

TYPED_TEST(VLATestsNonTrivialCommon, TestMoveToVector)
{
    cetl::VariableLengthArray<TypeParam, cetl::pf17::pmr::polymorphic_allocator<TypeParam>>
        subject{cetl::pf17::pmr::polymorphic_allocator<TypeParam>(cetl::pf17::pmr::new_delete_resource()), 10U};
    subject.reserve(subject.max_size());
    ASSERT_EQ(subject.capacity(), subject.max_size());
    for (std::size_t i = 0; i < subject.max_size(); ++i)
    {
        subject.push_back(static_cast<TypeParam>(i % 2));
        ASSERT_EQ(i + 1, subject.size());
    }
    std::vector<TypeParam> a(subject.cbegin(), subject.cend());
    for (std::size_t i = 0; i < subject.max_size(); ++i)
    {
        ASSERT_EQ(static_cast<TypeParam>(i % 2), a[i]);
    }
}

TYPED_TEST(VLATestsNonTrivialCommon, TestPushBackGrowsCapacity)
{
    static constexpr std::size_t                                                            PushBackItems = 9;
    cetl::VariableLengthArray<TypeParam, cetl::pf17::pmr::polymorphic_allocator<TypeParam>> subject{
        cetl::pf17::pmr::polymorphic_allocator<TypeParam>(cetl::pf17::pmr::new_delete_resource())};

    ASSERT_EQ(0U, subject.size());
    ASSERT_EQ(0U, subject.capacity());
    for (std::size_t i = 0; i < PushBackItems; ++i)
    {
        ASSERT_EQ(i, subject.size());
        ASSERT_LE(i, subject.capacity());
        subject.push_back(static_cast<TypeParam>(i));
        ASSERT_EQ(i + 1, subject.size());
        ASSERT_LE(i + 1, subject.capacity());
    }
    ASSERT_EQ(PushBackItems, subject.size());
    ASSERT_LE(PushBackItems, subject.capacity());
}

TYPED_TEST(VLATestsNonTrivialCommon, TestForEachConstIterators)
{
    static constexpr std::size_t MaxSize = 9;
    cetl::VariableLengthArray<TypeParam, cetl::pf17::pmr::polymorphic_allocator<TypeParam>>
          subject{cetl::pf17::pmr::polymorphic_allocator<TypeParam>(cetl::pf17::pmr::new_delete_resource()), MaxSize};
    const auto& const_subject = subject;
    ASSERT_EQ(0U, const_subject.size());
    ASSERT_EQ(0U, const_subject.capacity());
    for (const auto& item : const_subject)  // Requires begin() const, end() const.
    {
        (void) item;
        FAIL();
    }
    ASSERT_EQ(0U, const_subject.size());
    ASSERT_EQ(0U, const_subject.capacity());
    for (std::size_t i = 0; i < MaxSize; ++i)
    {
        ASSERT_EQ(i, const_subject.size());
        ASSERT_LE(i, const_subject.capacity());
        subject.push_back(static_cast<TypeParam>(i % 2));
        ASSERT_EQ(i + 1, const_subject.size());
        ASSERT_LE(i + 1, const_subject.capacity());
    }
    ASSERT_EQ(MaxSize, const_subject.size());
    ASSERT_LE(MaxSize, const_subject.capacity());
    std::size_t i = 0;
    for (const auto& item : const_subject)  // Requires begin() const, end() const.
    {
        ASSERT_EQ(static_cast<TypeParam>(i % 2), item);
        ++i;
    }
    ASSERT_EQ(const_subject.size(), i);
}

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif

TYPED_TEST(VLATestsNonTrivialCommon, SelfAssignment)
{
    auto allocator = cetl::pf17::pmr::polymorphic_allocator<TypeParam>(cetl::pf17::pmr::new_delete_resource());
    cetl::VariableLengthArray<TypeParam, cetl::pf17::pmr::polymorphic_allocator<TypeParam>> subject{allocator};
    subject.push_back(0);
    subject.push_back(1);
    ASSERT_EQ(2U, subject.size());
    subject = subject;
    ASSERT_EQ(2U, subject.size());
    ASSERT_EQ(0, subject[0]);
    ASSERT_EQ(1, subject[1]);
}

#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

TEST(VLATestsNonTrivialSpecific, TestDeallocSizeNonBool)
{
    cetlvast::InstrumentedAllocatorStatistics&          stats = cetlvast::InstrumentedAllocatorStatistics::get();
    cetlvast::InstrumentedNewDeleteAllocator<int>       allocator;
    cetl::VariableLengthArray<int, decltype(allocator)> subject{allocator};

    subject.reserve(10U);
    ASSERT_EQ(10U, subject.capacity());
    ASSERT_EQ(1U, stats.allocations);
    ASSERT_EQ(10U * sizeof(int), stats.last_allocation_size_bytes);
    ASSERT_EQ(0U, stats.last_deallocation_size_bytes);
    subject.pop_back();
    subject.shrink_to_fit();
    ASSERT_EQ(10U * sizeof(int), stats.last_deallocation_size_bytes);
}

TEST(VLATestsNonTrivialSpecific, TestPush)
{
    cetlvast::InstrumentedAllocatorStatistics&            stats = cetlvast::InstrumentedAllocatorStatistics::get();
    cetlvast::InstrumentedNewDeleteAllocator<std::size_t> allocator;
    cetl::VariableLengthArray<std::size_t, decltype(allocator)> subject{allocator};
    ASSERT_EQ(nullptr, subject.data());
    ASSERT_EQ(0U, subject.size());
    std::size_t x = 0;
    for (std::size_t i = 0; i < 1024U; ++i)
    {
        subject.push_back(x);
        ASSERT_EQ(i + 1, subject.size());
        ASSERT_LE(subject.size(), subject.capacity());
        const std::size_t* const pushed = &subject[i];
        ASSERT_EQ(*pushed, x);
        ++x;
    }
    // The container must not allocate for each push.
    ASSERT_GT(1024U, stats.allocations);
    subject.clear();
    ASSERT_EQ(0U, subject.size());
    ASSERT_GE(subject.capacity(), 1024U);
    subject.shrink_to_fit();
    ASSERT_EQ(0U, subject.capacity());
    ASSERT_EQ(0U, stats.outstanding_allocated_memory);
}

/**
 * Used to test that destructors were called.
 */
class Doomed
{
public:
    Doomed(int* out_signal_dtor)
        : out_signal_dtor_(out_signal_dtor)
        , moved_(false)
    {
    }
    Doomed(Doomed&& from) noexcept
        : out_signal_dtor_(from.out_signal_dtor_)
        , moved_(false)
    {
        from.moved_ = true;
    }
    Doomed(const Doomed&)            = delete;
    Doomed& operator=(const Doomed&) = delete;
    Doomed& operator=(Doomed&&)      = delete;

    ~Doomed()
    {
        if (!moved_)
        {
            (*out_signal_dtor_) += 1;
        }
    }

private:
    int* out_signal_dtor_;
    bool moved_;
};

TEST(VLATestsNonTrivialSpecific, TestDestroy)
{
    int dtor_called = 0;

    auto subject =
        std::make_shared<cetl::VariableLengthArray<Doomed, std::allocator<Doomed>>>(std::allocator<Doomed>{});

    subject->reserve(10);
    ASSERT_EQ(10U, subject->capacity());
    subject->push_back(Doomed(&dtor_called));
    ASSERT_EQ(1U, subject->size());
    subject->push_back(Doomed(&dtor_called));
    ASSERT_EQ(2U, subject->size());
    ASSERT_EQ(0, dtor_called);
    subject.reset();
    ASSERT_EQ(2, dtor_called);
}

TEST(VLATestsNonTrivialSpecific, TestNonFundamental)
{
    int dtor_called = 0;

    cetl::VariableLengthArray<Doomed, std::allocator<Doomed>> subject(std::allocator<Doomed>{});

    subject.reserve(10U);
    ASSERT_EQ(10U, subject.capacity());
    subject.push_back(Doomed(&dtor_called));
    ASSERT_EQ(1U, subject.size());
    subject.pop_back();
    ASSERT_EQ(1, dtor_called);
}

TEST(VLATestsNonTrivialSpecific, TestNotMovable)
{
    class NotMovable
    {
    public:
        NotMovable() {}
        NotMovable(NotMovable&&) = delete;
        NotMovable(const NotMovable& rhs) noexcept
        {
            (void) rhs;
        }
    };
    cetl::VariableLengthArray<NotMovable, std::allocator<NotMovable>> subject(std::allocator<NotMovable>{});

    subject.reserve(10U);
    ASSERT_EQ(10U, subject.capacity());
    NotMovable source;
    subject.push_back(source);
    ASSERT_EQ(1U, subject.size());
}

TEST(VLATestsNonTrivialSpecific, TestMovable)
{
    class Movable
    {
    public:
        Movable(int data)
            : data_(data)
        {
        }
        Movable(const Movable&) = delete;
        Movable(Movable&& move_from) noexcept
            : data_(move_from.data_)
        {
            move_from.data_ = 0;
        }
        int get_data() const
        {
            return data_;
        }

    private:
        int data_;
    };
    cetl::VariableLengthArray<Movable, std::allocator<Movable>> subject(std::allocator<Movable>{});
    subject.reserve(10U);
    ASSERT_EQ(10U, subject.capacity());
    subject.push_back(Movable(1));
    ASSERT_EQ(1U, subject.size());
    Movable* pushed = &subject[0];
    ASSERT_NE(nullptr, pushed);
    ASSERT_EQ(1, pushed->get_data());
}

TEST(VLATestsNonTrivialSpecific, TestInitializerArray)
{
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> subject{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
                                                                                std::allocator<std::size_t>{}};
    ASSERT_EQ(10U, subject.size());
    for (std::size_t i = 0; i < subject.size(); ++i)
    {
        ASSERT_EQ(subject.size() - i, subject[i]);
    }
}

TEST(VLATestsNonTrivialSpecific, TestCopyConstructor)
{
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> fixture{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
                                                                                std::allocator<std::size_t>{}};

    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> subject(fixture);
    ASSERT_EQ(10U, subject.size());
    for (std::size_t i = 0; i < subject.size(); ++i)
    {
        ASSERT_EQ(subject.size() - i, subject[i]);
    }
}

TEST(VLATestsNonTrivialSpecific, TestMoveConstructor)
{
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> fixture{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
                                                                                std::allocator<std::size_t>{}};

    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> subject(std::move(fixture));
    ASSERT_EQ(10U, subject.size());
    for (std::size_t i = 0; i < subject.size(); ++i)
    {
        ASSERT_EQ(subject.size() - i, subject[i]);
    }
    ASSERT_EQ(0U, fixture.size());
    ASSERT_EQ(0U, fixture.capacity());
}

TEST(VLATestsNonTrivialSpecific, TestCompare)
{
    std::allocator<std::size_t>                                         allocator{};
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> one{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1}, allocator};
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> two{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1}, allocator};
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> three{{9, 8, 7, 6, 5, 4, 3, 2, 1}, allocator};
    ASSERT_EQ(one, one);
    ASSERT_EQ(one, two);
    ASSERT_NE(one, three);
}

TEST(VLATestsNonTrivialSpecific, TestFPCompare)
{
    std::allocator<std::size_t>                                    allocator{};
    cetl::VariableLengthArray<double, std::allocator<std::size_t>> one{{1.00, 2.00}, allocator};
    cetl::VariableLengthArray<double, std::allocator<std::size_t>> two{{1.00, 2.00}, allocator};
    const double epsilon_for_two_comparison = std::nextafter(4.00, INFINITY) - 4.00;
    cetl::VariableLengthArray<double, std::allocator<std::size_t>>
        three{{1.00, std::nextafter(2.00 + epsilon_for_two_comparison, INFINITY)}, allocator};
    ASSERT_EQ(one, one);
    ASSERT_EQ(one, two);
    ASSERT_NE(one, three);
}

TEST(VLATestsNonTrivialSpecific, TestCompareBool)
{
    std::allocator<bool>                                  allocator{};
    cetl::VariableLengthArray<bool, std::allocator<bool>> one{{true, false, true}, allocator};
    cetl::VariableLengthArray<bool, std::allocator<bool>> two{{true, false, true}, allocator};
    cetl::VariableLengthArray<bool, std::allocator<bool>> three{{true, true, false}, allocator};
    ASSERT_EQ(one, one);
    ASSERT_EQ(one, two);
    ASSERT_NE(one, three);
}

TEST(VLATestsNonTrivialSpecific, TestCopyAssignment)
{
    std::allocator<double>                                    allocator{};
    cetl::VariableLengthArray<double, std::allocator<double>> lhs{{1.00}, allocator};
    cetl::VariableLengthArray<double, std::allocator<double>> rhs{{2.00, 3.00}, allocator};
    ASSERT_EQ(1U, lhs.size());
    ASSERT_EQ(2U, rhs.size());
    ASSERT_NE(lhs, rhs);
    lhs = rhs;
    ASSERT_EQ(2U, lhs.size());
    ASSERT_EQ(2U, rhs.size());
    ASSERT_EQ(lhs, rhs);
}

TEST(VLATestsNonTrivialSpecific, TestMoveAssignment)
{
    std::allocator<std::string>                                         allocator{};
    cetl::VariableLengthArray<std::string, std::allocator<std::string>> lhs{{std::string("one"), std::string("two")},
                                                                            allocator};
    cetl::VariableLengthArray<std::string, std::allocator<std::string>> rhs{{std::string("three"),
                                                                             std::string("four"),
                                                                             std::string("five")},
                                                                            allocator};
    ASSERT_EQ(2U, lhs.size());
    ASSERT_EQ(3U, rhs.size());
    ASSERT_NE(lhs, rhs);
    lhs = std::move(rhs);
    ASSERT_EQ(3U, lhs.size());
    ASSERT_EQ(0U, rhs.size());
    ASSERT_EQ(0U, rhs.capacity());
    ASSERT_NE(lhs, rhs);
    ASSERT_EQ(std::string("three"), lhs[0]);
}

struct NoDefault
{
    ~NoDefault() = default;
    NoDefault()  = delete;
    NoDefault(int value)
        : data_{value} {};
    NoDefault(const NoDefault&)                = default;
    NoDefault(NoDefault&&) noexcept            = default;
    NoDefault& operator=(const NoDefault&)     = default;
    NoDefault& operator=(NoDefault&&) noexcept = default;

    int data() const noexcept
    {
        return data_;
    }

private:
    int data_;
};

TEST(VLATestsNonTrivialSpecific, TestResizeWithNoDefaultCtorData)
{
    std::allocator<NoDefault>                                       allocator{};
    cetl::VariableLengthArray<NoDefault, std::allocator<NoDefault>> subject{{NoDefault{1}}, allocator};
    ASSERT_EQ(1, subject.size());
    subject.resize(10, NoDefault{2});
    ASSERT_EQ(10, subject.size());
    ASSERT_EQ(1, subject[0].data());
    for (std::size_t i = 1; i < subject.size(); ++i)
    {
        ASSERT_EQ(2, subject[i].data());
    }
}

#ifdef __cpp_exceptions

struct GrenadeError : public std::runtime_error
{
    GrenadeError(const char* message)
        : std::runtime_error(message)
    {
    }
};

struct Grenade
{
    Grenade(int value)
        : value_{value}
    {
    }

    Grenade(const Grenade& rhs)
        : value_{rhs.value_}
    {
        if (value_ == 2)
        {
            throw GrenadeError("Kaboom!");
        }
    }
private:
    int value_;
};

TYPED_TEST(VLATestsGeneric, TestResizeExceptionFromCtorOnResize)
{
    std::allocator<Grenade>                                     allocator{};
    cetl::VariableLengthArray<Grenade, std::allocator<Grenade>> subject{{Grenade{1}}, allocator};
    ASSERT_EQ(1, subject.size());
    ASSERT_THROW(subject.resize(2, Grenade{2}), GrenadeError);
}

#endif  // __cpp_exceptions
