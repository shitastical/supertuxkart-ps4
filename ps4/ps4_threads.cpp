// PS4 pthread attributes and recursive mutexes.
//
// libkernel does not keep a pthread_mutexattr_t in the caller's variable: pthread_mutexattr_init
// takes an attribute object from a kernel pool and stores its address in the caller's 8-byte
// pthread_mutexattr_t. On retail firmware that pool is not reliable once the process heap exists -
// the init returns an error and leaves the caller's variable untouched. Both SDL (SDL_CreateMutex,
// from SDL_TimerInit) and libc++ (__libcpp_recursive_mutex_init) ignore the error and then pass the
// uninitialised attribute to pthread_mutexattr_settype, which dereferences it inside libkernel: the
// SuperTuxKart crash at rip 0x800009fdc, thread stopped in SDL_CreateMutex.
//
// SDL is built without the attribute API on PS4 (see SDL_sysmutex.c: it emulates recursion itself).
// This file covers everything else that goes through pthread directly:
//
//   - An attribute object the kernel refused is replaced by one of ours, so no caller ever holds an
//     uninitialised attribute, and settype/destroy are honest no-ops on it.
//   - The type requested with pthread_mutexattr_settype is remembered per attribute. Once the
//     attribute pool has failed, mutexes are created with no attributes at all (a plain kernel
//     mutex), and pthread_mutex_lock/unlock/trylock emulate the recursion for the mutexes that asked
//     for PTHREAD_MUTEX_RECURSIVE. libc++'s std::recursive_mutex (STK's file manager) keeps working.
//   - With a healthy attribute pool nothing changes: the kernel gets the same objects it made.
//
// Every decision is written to boot-trace.log, so the log names the failure instead of leaving a
// crash log that points into libkernel.

#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern "C" void ps4_boot_tracef(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

extern "C" {
int __real_pthread_mutexattr_init(pthread_mutexattr_t* attr);
int __real_pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int __real_pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int __real_pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int __real_pthread_mutex_lock(pthread_mutex_t* mutex);
int __real_pthread_mutex_unlock(pthread_mutex_t* mutex);
int __real_pthread_mutex_trylock(pthread_mutex_t* mutex);
int __real_pthread_mutex_destroy(pthread_mutex_t* mutex);
}

namespace {

constexpr int kRecursive = 2;  // PTHREAD_MUTEX_RECURSIVE (FreeBSD, and the SDK's pthread.h)

// The 8-byte handle is opaque; read and write it without naming the header's member.
static_assert(sizeof(pthread_mutexattr_t) >= sizeof(void*), "the kernel stores a pointer here");

void* attrGet(const pthread_mutexattr_t* attr) {
    void* pointer = nullptr;
    memcpy(&pointer, attr, sizeof pointer);
    return pointer;
}

void attrSet(pthread_mutexattr_t* attr, void* pointer) {
    memcpy(attr, &pointer, sizeof pointer);
}

// One spin flag for the three tables below: the wrappers run before threads exist and from
// libc++'s and SDL's own code, so they must not take a pthread lock (and must not allocate).
int g_tableLock;

void lockTables() {
    while (__atomic_test_and_set(&g_tableLock, __ATOMIC_ACQUIRE)) {
    }
}

void unlockTables() {
    __atomic_clear(&g_tableLock, __ATOMIC_RELEASE);
}

// Attribute objects of our own, for the calls libkernel refuses. Handed out as the same opaque
// pointer the kernel would have stored; layout (type, protocol, ceiling) matches what the kernel
// reads if it is ever handed one back. Only touched while the pool is unusable.
struct AttrObject {
    uint32_t type;
    uint32_t protocol;
    int32_t ceiling;
    int used;
};

constexpr int kAttrObjects = 64;
AttrObject g_attrObjects[kAttrObjects];

AttrObject* ownAttr(void* pointer) {
    for (int i = 0; i < kAttrObjects; i++) {
        if (pointer == &g_attrObjects[i])
            return &g_attrObjects[i];
    }
    return nullptr;
}

AttrObject* allocAttr() {
    lockTables();
    AttrObject* found = nullptr;
    for (int i = 0; i < kAttrObjects; i++) {
        if (!g_attrObjects[i].used) {
            found = &g_attrObjects[i];
            memset(found, 0, sizeof *found);
            found->used = 1;
            break;
        }
    }
    unlockTables();
    return found;
}

void freeAttr(AttrObject* object) {
    lockTables();
    object->used = 0;
    unlockTables();
}

// The type requested for an attribute. Kernel attributes are remembered by address (settype is the
// only call that sets a type, and destroy drops the entry); our own objects carry it themselves.
struct AttrType {
    void* attr;
    int type;
    int used;
};

constexpr int kAttrTypes = 64;
AttrType g_attrTypes[kAttrTypes];

void rememberType(void* attr, int type) {
    lockTables();
    AttrType* free_entry = nullptr;
    for (int i = 0; i < kAttrTypes; i++) {
        AttrType& entry = g_attrTypes[i];
        if (entry.used && entry.attr == attr) {
            entry.type = type;
            unlockTables();
            return;
        }
        if (!entry.used && !free_entry)
            free_entry = &entry;
    }
    if (free_entry) {
        free_entry->attr = attr;
        free_entry->type = type;
        free_entry->used = 1;
    }
    unlockTables();
}

int typeOf(void* attr) {
    lockTables();
    int type = 0;
    for (int i = 0; i < kAttrTypes; i++) {
        if (g_attrTypes[i].used && g_attrTypes[i].attr == attr) {
            type = g_attrTypes[i].type;
            break;
        }
    }
    unlockTables();
    return type;
}

void forgetType(void* attr) {
    lockTables();
    for (int i = 0; i < kAttrTypes; i++) {
        if (g_attrTypes[i].used && g_attrTypes[i].attr == attr)
            g_attrTypes[i].used = 0;
    }
    unlockTables();
}

// Recursive mutexes emulated over plain kernel mutexes, used only after the attribute pool failed.
struct RecursiveMutex {
    pthread_mutex_t* mutex;
    pthread_t owner;
    unsigned depth;
};

constexpr int kRecursiveMutexes = 64;
RecursiveMutex g_recursiveMutexes[kRecursiveMutexes];

RecursiveMutex* findRecursive(pthread_mutex_t* mutex) {
    for (int i = 0; i < kRecursiveMutexes; i++) {
        if (g_recursiveMutexes[i].mutex == mutex)
            return &g_recursiveMutexes[i];
    }
    return nullptr;
}

void addRecursive(pthread_mutex_t* mutex) {
    lockTables();
    for (int i = 0; i < kRecursiveMutexes; i++) {
        RecursiveMutex& entry = g_recursiveMutexes[i];
        if (!entry.mutex) {
            entry.mutex = mutex;
            entry.owner = 0;
            entry.depth = 0;
            unlockTables();
            return;
        }
    }
    unlockTables();
}

void removeRecursive(pthread_mutex_t* mutex) {
    lockTables();
    for (int i = 0; i < kRecursiveMutexes; i++) {
        if (g_recursiveMutexes[i].mutex == mutex)
            memset(&g_recursiveMutexes[i], 0, sizeof g_recursiveMutexes[i]);
    }
    unlockTables();
}

// Set once libkernel has refused an attribute: from then on no attribute object is handed to the
// kernel (the caller's variable may be untouched or ours, neither is a kernel object).
int g_attrPoolBroken;
int g_logged;
int g_inLog;

void note(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void note(const char* fmt, ...) {
    if (!g_attrPoolBroken && g_logged >= 8)
        return;
    if (g_inLog || g_logged >= 32)
        return;
    g_inLog = 1;
    g_logged++;
    char text[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
    ps4_boot_tracef("[ps4] %s", text);
    g_inLog = 0;
}

}  // namespace

extern "C" int __wrap_pthread_mutexattr_init(pthread_mutexattr_t* attr) {
    if (!attr)
        return 22;  // EINVAL
    attrSet(attr, nullptr);
    const int result = __real_pthread_mutexattr_init(attr);
    void* pointer = attrGet(attr);
    if (result == 0 && pointer)
        return 0;
    // The kernel refused it (or filled in nothing). Give the caller an object of ours: the
    // alternative is what crashed - a caller that ignores the error and dereferences it.
    const bool first = !g_attrPoolBroken;
    g_attrPoolBroken = 1;
    if (AttrObject* object = allocAttr()) {
        attrSet(attr, object);
        if (first)
            note("pthread_mutexattr_init: kernel returned %d, attributes emulated", result);
        return 0;
    }
    attrSet(attr, nullptr);
    note("pthread_mutexattr_init: kernel returned %d, no attribute object left", result);
    return result ? result : 12;  // ENOMEM
}

extern "C" int __wrap_pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
    if (!attr)
        return 22;
    void* pointer = attrGet(attr);
    if (AttrObject* object = ownAttr(pointer)) {
        object->type = static_cast<uint32_t>(type);
        return 0;
    }
    if (!pointer)
        return 22;
    rememberType(pointer, type);
    const int result = __real_pthread_mutexattr_settype(attr, type);
    if (result != 0) {
        // The attribute exists but the kernel rejected the type; the mutex wrapper still applies
        // it, because it remembers what the caller asked for.
        note("pthread_mutexattr_settype(%d): kernel returned %d, kept for the mutex", type, result);
        return 0;
    }
    return 0;
}

extern "C" int __wrap_pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
    if (!attr)
        return 22;
    void* pointer = attrGet(attr);
    if (AttrObject* object = ownAttr(pointer)) {
        freeAttr(object);
        attrSet(attr, nullptr);
        return 0;
    }
    if (!pointer)
        return 0;
    forgetType(pointer);
    return __real_pthread_mutexattr_destroy(attr);
}

extern "C" int __wrap_pthread_mutex_init(pthread_mutex_t* mutex,
                                          const pthread_mutexattr_t* attr) {
    if (!mutex)
        return 22;
    const void* pointer = attr ? attrGet(attr) : nullptr;
    int type = 0;
    if (const AttrObject* object = ownAttr(const_cast<void*>(pointer)))
        type = static_cast<int>(object->type);
    else if (pointer)
        type = typeOf(const_cast<void*>(pointer));

    // With the attribute pool unusable the kernel is never given an attribute: a caller's variable
    // may hold nothing at all. The type is applied here instead.
    const bool plain = g_attrPoolBroken;
    const int result = __real_pthread_mutex_init(mutex, plain ? nullptr : attr);
    if (result != 0) {
        if (plain) {
            note("pthread_mutex_init: kernel returned %d for a plain mutex", result);
        } else {
            // Do not fail the caller on an attribute the kernel cannot use: retry plain.
            g_attrPoolBroken = 1;
            const int retry = __real_pthread_mutex_init(mutex, nullptr);
            note("pthread_mutex_init: kernel returned %d, plain retry returned %d", result, retry);
            if (retry != 0)
                return retry;
        }
    }
    if (type == kRecursive)
        addRecursive(mutex);
    return 0;
}

extern "C" int __wrap_pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (!mutex)
        return 22;
    RecursiveMutex* entry = findRecursive(mutex);
    if (entry) {
        const pthread_t self = pthread_self();
        if (entry->depth != 0 && pthread_equal(entry->owner, self)) {
            entry->depth++;
            return 0;
        }
    }
    const int result = __real_pthread_mutex_lock(mutex);
    if (result == 0 && entry) {
        entry->owner = pthread_self();
        entry->depth = 1;
    }
    return result;
}

extern "C" int __wrap_pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if (!mutex)
        return 22;
    RecursiveMutex* entry = findRecursive(mutex);
    if (entry) {
        const pthread_t self = pthread_self();
        if (entry->depth != 0 && pthread_equal(entry->owner, self)) {
            entry->depth++;
            return 0;
        }
    }
    const int result = __real_pthread_mutex_trylock(mutex);
    if (result == 0 && entry) {
        entry->owner = pthread_self();
        entry->depth = 1;
    }
    return result;
}

extern "C" int __wrap_pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (!mutex)
        return 22;
    RecursiveMutex* entry = findRecursive(mutex);
    if (entry) {
        if (entry->depth > 1) {
            entry->depth--;
            return 0;
        }
        entry->depth = 0;
        entry->owner = 0;
    }
    return __real_pthread_mutex_unlock(mutex);
}

extern "C" int __wrap_pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (!mutex)
        return 22;
    removeRecursive(mutex);
    return __real_pthread_mutex_destroy(mutex);
}

// Runs after the kit's own diagnostics (priority 101) have opened boot-trace.log, and before any of
// STK's static constructors. Measures the attribute API on this firmware instead of guessing.
__attribute__((constructor(200))) static void ps4_threads_probe() {
    pthread_mutexattr_t attr;
    memset(&attr, 0, sizeof attr);
    const int initResult = __real_pthread_mutexattr_init(&attr);
    void* pointer = attrGet(&attr);
    ps4_boot_tracef("[ps4] pthread attribute probe: init rc=%d attr=%p", initResult, pointer);
    if (initResult == 0 && pointer) {
        const int typeResult = __real_pthread_mutexattr_settype(&attr, kRecursive);
        pthread_mutex_t probe;
        memset(&probe, 0, sizeof probe);
        const int mutexResult = __real_pthread_mutex_init(&probe, &attr);
        ps4_boot_tracef("[ps4] pthread attribute probe: settype rc=%d mutex_init rc=%d",
                        typeResult, mutexResult);
        if (mutexResult == 0)
            __real_pthread_mutex_destroy(&probe);
        __real_pthread_mutexattr_destroy(&attr);
    } else {
        // From here on no attribute object of the kernel's is handed back to it, and every
        // refusal is logged (the pool is known to be unusable, not merely unlucky once).
        g_attrPoolBroken = 1;
        ps4_boot_tracef("[ps4] pthread attribute pool unusable; recursive mutexes will be "
                        "emulated over plain ones");
    }
}
