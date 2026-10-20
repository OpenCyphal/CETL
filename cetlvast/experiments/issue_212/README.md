# Efficient resizing of trivial data in VLA

Implementation of [CETL issue 212](https://github.com/OpenCyphal/CETL/issues/212),
built on CETL revision `3f1f553`. VLA uses CETL's existing allocator `reallocate` extension in its
copy-assignment growth path, retaining its restriction to trivially copyable storage. The PMR
interfaces and contracts stay unchanged. Non-trivial element types retain their existing path.

The value proposition is contiguous storage for trivial data with familiar `std::vector` ergonomics
and inexpensive resizing when the allocator can extend the existing block. With the buddy resource,
successful in-place growth updates allocator metadata without copying any existing payload bytes.
This approaches the storage efficiency of a fixed `std::array` while allowing the logical array and
its capacity to grow. VLA still has runtime size/capacity bookkeeping and allocator overhead; the
example demonstrates avoided work and memory use, not timing equivalence with `std::array`.

The accompanying [buddy resource example](../../suites/docs/examples/example_12_buddy_memory_resource.cpp)
implements a fixed arena and already demonstrates this behavior through `reserve` and `push_back`.
Copy assignment now uses the same mechanism when its destination needs more capacity. The example
includes both successful in-place assignment growth and fallback when a buddy is occupied.

## Existing support and scope

VLA already detects `allocator.reallocate(pointer, old_count, new_count)` and uses it for eligible
storage in `reserve` and `shrink_to_fit`. The existing helper combines allocator-member detection
with `std::is_trivially_copyable<value_type>` and returns null when either condition is false.
Packed `bool` qualifies through its underlying byte storage. Copy assignment reuses this helper
with the same type restriction, including for empty buffers of non-trivial elements.

`copy_assign_from` reuses compatible storage when the source fits. When the source requires more
capacity, it first attempts reallocation of eligible destination storage. If this is unavailable or
declined, it destroys and releases the destination buffer, propagates the allocator when required,
reserves replacement storage, and copy-constructs the incoming elements.

Implementation changes belong in `include/cetl/variable_length_array.hpp`, its allocation tests,
and the documented example. No new resource virtual functions, allocator members, traits APIs,
or non-trivial object optimizations are required. Move assignment and range assignment are outside
this change. Preserve the existing alias-safe append behavior.

## Copy assignment algorithm

Self-assignment handling, size-bound propagation, and the sufficient-capacity branch retain their
existing behavior. The branch that previously always discarded destination storage now works as follows:

1. Determine whether storage remains compatible: POCCA is false, the allocators compare equal,
   or `is_always_equal` is true. POCCA means `propagate_on_container_copy_assignment`.
2. When compatible storage exists and the source requires growth, resolve the same target size and
   bound checks as the current reserve path. Use the existing VLA helper to attempt
   `reallocate(data_, alloc_, capacity_, target)`. For non-trivial elements or allocators without
   the extension, the helper returns null without calling the allocator.
3. On success, adopt the **returned pointer** and target capacity. Do not deallocate the old pointer:
   the allocator has already handled the allocation transition, whether it moved or stayed in place.
   Set the destination's logical size to zero before constructing the incoming range, perform any
   required allocator propagation, and use the existing `fast_copy_construct` and rollback machinery.
   Set `size_` from the completed construction result. There is no element-destruction work needed
   before reallocating these trivially copyable elements.
4. On a null result, missing extension, empty destination, or incompatible propagated allocator,
   execute the current release-then-allocate branch. Deallocate using the original allocator and
   original capacity, then propagate the allocator and obtain replacement storage. A null realloc
   result leaves the old buffer valid, so that existing cleanup remains correct.

Do not route this through ordinary `reserve` on the populated destination: when realloc is declined,
its fallback would allocate replacement storage and preserve old values that assignment will overwrite.
The assignment fallback should continue releasing the old destination before allocating its replacement.

Retain the existing basic exception guarantee. If an allocator exception propagates out of the realloc
attempt, the original allocation remains owned by the destination. If replacement allocation fails
after release, the destination is empty with no allocation. If construction into successful storage
throws, the existing rollback helper destroys the constructed prefix and leaves an empty destination
with a valid buffer. Allocator propagation and destruction must continue using the appropriate owner.
The existing PMR traits forwarding function is `noexcept`; this change does not alter its handling
of a throwing resource. The example resource declines reallocation by returning null.

Packed `bool` passes sizes and the maximum size to the base helper in **storage bytes**. Its assignment
wrapper maintains logical bit count and final-byte occupancy, including partial-byte boundaries and
failures. With exceptions disabled, a copy truncated to the allocator's byte limit ends at a full
byte; failed replacement allocation resets the final-byte count along with the emptied base.

## Allocator behavior and memory cost

The existing extension permits reallocation to move the block. VLA must handle both outcomes; it
must not assume that success means the address is unchanged. The example resource implements a
useful narrower policy: merge free buddies in place, or return null and let VLA choose its fallback.
That is an implementation choice within the existing extension, not a new PMR contract.

For physical source reservation `S`, old destination reservation `D`, and new destination reservation
`N`, successful in-place growth or release-then-allocate has peak payload reservation
`S + max(D, N)`, including the initial state. A resource that implements reallocation by allocating,
copying, and freeing can temporarily reach `S + D + N` and copy old destination bytes that will be
overwritten. Document this allocator-dependent trade-off rather than adding an interface to prohibit
it. Buddy rounding affects these physical sizes; resource metadata is additional.

Distinguish the two demonstrations. For ordinary growth, in-place reallocation preserves existing
trivial data without copying it. For copy assignment, reallocation saves buffer replacement work,
but source data still has to be copied into the destination. Neither case requires non-trivial
object support to demonstrate the benefit.

## Buddy resource example

The example uses the binary split/coalesce pattern in the supplied
[buddy allocation description](https://en.wikipedia.org/wiki/Buddy_memory_allocation).
It has a 1024-byte aligned arena, a 16-byte minimum block, and a separate fixed binary-tree array
whose nodes are free, split, or allocated. Payload storage and metadata require no upstream heap.
Alignment is supported through `alignof(std::max_align_t)`; unsupported alignment is rejected.

Allocation rounds up to a power of two and splits free blocks, choosing the lower half first.
Deallocation repeatedly coalesces a block with its free sibling. For a block of size `B` at offset
`O` relative to the arena, its buddy offset is `O ^ B`. Address-preserving growth requires
`(O & B) == 0` and a free buddy at each level up to the desired size. Using offsets avoids requiring
the arena's absolute address to be aligned to its entire size.

```text
Before  16 bytes used | 16 free |        32 free        | ...
Grow    [          32 used    ]|        32 free        | ...
Grow    [                     64 used                 ] ...
        ^ the allocation starts at the same address throughout
```

The resource preflights the entire ancestor path before committing any merges. It declines growth
of an upper buddy even if its lower sibling is free: combining them would change the start address.
Shrinkage keeps the lower portion and returns upper buddies to the tree. Requests within the same
physical block update the caller's requested size without changing physical reservation.

Allocation in this small teaching implementation searches the tree and can take O(number of
nodes); merging and splitting take O(log(arena size / minimum block size)). Bookkeeping uses
127 one-byte nodes plus statistics and alignment padding. This example does not claim O(1)
allocation, a production fragmentation bound, or a measured execution-time speedup.

### Demonstration on the current API

The resource's existing `do_reallocate` performs only metadata changes or returns null. With `char`
elements the current VLA can already use it safely. Explicitly use the CETL allocator:

```cpp
using allocator = cetl::pf17::pmr::polymorphic_allocator<char>;
buddy_memory_resource arena;
cetl::variable_length_array<char, allocator> values{allocator{&arena}};
values.push_back('x');
values.reserve(64);
values.push_back('y');
```

The same staged `reserve`/`push_back` function runs against VLA and `std::vector` on separate
identical arenas. It reserves 16, 32, 64, 128, and 256 elements and fills each stage. CETL uses one
allocation, four buddy merges, no intermediate deallocation, and 256 peak reserved payload bytes.
`std::vector` does not call the extension and needs replacement storage during growth. On the
tested libc++ implementation it uses five allocations and 384 peak bytes. The example reports
these measurements rather than prescribing the vector's capacities: a library may overreserve
and skip a later stage, so only the 256-byte final payload is a portable lower bound here.
Counters exclude final destruction until the containers leave scope. A separate append-only case
checks unchanged numeric addresses and preserved values through implicit VLA growth.

The general resource extension's invalidation rules still apply to this demonstration. It records
numeric addresses for comparison; applications must not infer a portable iterator-validity
guarantee from this particular allocator. The resource's zero payload writes during reallocation
and successful merge counters establish why the existing elements need no relocation.

### Copy assignment example for issue 212

`copy_assignment_combines_free_buddies` creates a 16-character destination and a 64-character
source on separate arenas, then executes `destination = source`. It verifies two buddy merges,
an unchanged numeric address, no replacement allocation or intermediate deallocation, copied
source values, and 64 peak destination arena bytes. Source contents remain unchanged.

`copy_assignment_releases_before_fallback_allocation` occupies the destination's 16-byte buddy
before assigning the larger source. The resource declines realloc, and VLA releases the old
16-byte destination before allocating its 64-byte replacement. The peak is 80 bytes including
the 16-byte blocker; simultaneous old and new destination allocations would require 96 bytes.
Both cases check that all arena storage is released when the containers leave scope.

## Regression coverage

The `VLACopyAssignmentReallocation` tests in
`test_variable_length_array_detailed_allocation.cpp` cover:

- In-place success, moving success, and null-return fallback with explicit CETL PMR, for scalars
  and trivially copyable records. Allocation events verify returned-pointer adoption, release
  ordering, byte counts, alignment, and final ownership.
- Non-propagating unequal allocators, propagating equal and unequal allocators, and always-equal
  allocators. Distinct allocator identity values make propagation observable even when equal.
- Allocators without the extension, and non-trivial elements with both populated and empty
  reserved destinations. These retain the existing release-then-allocate behavior.
- Self-assignment, sufficient capacity, empty sources, empty destinations, bound propagation,
  and a reduced allocator limit. Oversized requests reach the existing throw/clamp behavior
  without an out-of-bounds live-buffer realloc attempt.
- Packed `bool` across byte boundaries, reduced allocator byte limits, and subsequent append; failed allocation with exceptions
  enabled or disabled; a direct allocator throwing from realloc; and construction-hook failure
  at the first, middle, or final element after successful reallocation.

The existing VLA regressions cover reserve, shrink, aliasing, assignment, non-trivial object
lifetimes, and move behavior. The buddy example remains registered in CMake and the Doxygen index.
Its performance claim is avoided payload relocation and allocation work under favorable buddy
availability, with measured allocator counts and memory consumption.

## Running the current example

After configuring CETLVaSt, build and run the registered example target from the repository root:

```sh
cmake --build cetlvast/build --config Debug --target example_12_buddy_memory_resource
cetlvast/build/suites/docs/examples/Debug/example_12_buddy_memory_resource
cmake --build cetlvast/build --config DebugEP --target example_12_buddy_memory_resource
cetlvast/build/suites/docs/examples/DebugEP/example_12_buddy_memory_resource
```

The example has ten tests, including both copy-assignment cases. Local validation passed:

- C++14 Debug: 1,210 tests across all 17 VLA executables and the buddy example.
- C++14 DebugEP, with exceptions and RTTI disabled: 1,156 tests across the same targets.
- C++17 with AddressSanitizer and UndefinedBehaviorSanitizer: 597 tests covering the detailed
  allocation suite and buddy example, built together with a matching test-framework toolchain.
