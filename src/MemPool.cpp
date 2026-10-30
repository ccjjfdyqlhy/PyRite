// A size-class free-list in front of malloc.
//
// The interpreter churns through huge numbers of tiny allocations: every
// arithmetic result is a shared_ptr-managed NumberValue, every list node
// and every short string goes through the global allocator. glibc malloc
// is fast, but a fixed set of size classes that never leaves the cache
// measures better still - and the runtime is single-threaded, so the
// free lists need no synchronization.
//
// Every allocation carries a one-granule header holding its size class;
// that is what lets delete() route the block back to the right free
// list (or to free() for oversized blocks) with no other bookkeeping.

#include <cstdlib>
#include <new>

namespace {

    constexpr size_t GRANULE = 16;
    constexpr size_t MAX_POOLLED = 256;                  // largest size we cache
    constexpr size_t CLASSES = MAX_POOLLED / GRANULE;    // 16 buckets
    constexpr size_t LOW_WATER = 64;                     // keep at most this many per class
    constexpr size_t HEADER = GRANULE;                   // keeps returned pointers 16-aligned

    struct FreeNode { FreeNode* next; };
    struct Header { size_t cls; };                       // cls == CLASSES means "not pooled"

    struct Pool {
        FreeNode* head = nullptr;
        size_t count = 0;
    };

    Pool pools[CLASSES];

    inline size_t size_class(size_t n) {
        // n >= 1: index of the smallest bucket that fits
        return (n + GRANULE - 1) / GRANULE - 1;
    }

} // namespace

void* operator new(size_t sz) {
    // 0-based size class, stored verbatim in the header so delete()
    // can index the same bucket without translating.
    size_t cls = (sz + GRANULE - 1) / GRANULE - 1;
    if (cls < CLASSES) {
        Pool& p = pools[cls];
        if (p.head) {
            FreeNode* node = p.head;
            p.head = node->next;
            p.count--;
            return node;                          // header below it still holds cls
        }
    } else {
        cls = CLASSES;                            // mark "not pooled"
    }
    // Allocate the full class size, never the exact request: a block
    // recycled into a class must serve the largest size in that class.
    size_t bytes = cls < CLASSES ? (cls + 1) * GRANULE + HEADER : sz + HEADER;
    void* block = std::malloc(bytes);
    if (!block) throw std::bad_alloc();
    static_cast<Header*>(block)->cls = cls;
    return static_cast<char*>(block) + HEADER;
}

void operator delete(void* ptr) noexcept {
    if (!ptr) return;
    char* base = static_cast<char*>(ptr) - HEADER;
    size_t cls = static_cast<Header*>(static_cast<void*>(base))->cls;
    if (cls < CLASSES) {
        Pool& p = pools[cls];
        if (p.count < LOW_WATER) {
            FreeNode* node = static_cast<FreeNode*>(ptr);
            node->next = p.head;
            p.head = node;
            p.count++;
            return;
        }
    }
    std::free(base);
}

void* operator new[](size_t sz) { return ::operator new(sz); }
void operator delete[](void* ptr) noexcept { ::operator delete(ptr); }
void operator delete(void* ptr, size_t) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, size_t) noexcept { ::operator delete(ptr); }
void* operator new(size_t sz, const std::nothrow_t&) noexcept { try { return ::operator new(sz); } catch (...) { return nullptr; } }
void operator delete(void* ptr, const std::nothrow_t&) noexcept { ::operator delete(ptr); }
