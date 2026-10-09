// KERNEL32 on POSIX: the subset the game and minwin_impl use (tools/minwin_inventory.txt), declared by port/minwin/kernel32.h.
//
// Handles are pointers to small heap objects (events, semaphores, threads, find handles); every wait goes through one process-wide
// mutex and condition variable, which is plenty for the game's few synchronisation points (the sound refill thread, the movie
// thread). Files are plain descriptors; Windows-style path names (drive letters, backslashes, any case) are translated by the
// resource library's ResolvePath.
#define MINWIN_IMPLEMENTATION 1
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <unordered_set>

#include "port/minwin/minwin.h"
#include "port/minwin_impl/res/path.h"
#include "port/native/native_clock.h"
#include "port/native/platform.h"

namespace {

enum class Kind { Event, Semaphore, Thread, Find };

// The objects that exist: a handle is a pointer, and the game (like any Win32 program) can use a handle after closing it -- the
// DirectSound mixer signals notification events its client has closed -- which must fail, not touch freed memory.
struct LiveObjects {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    std::unordered_set<const void*> set;
};

LiveObjects& Live() {
    static LiveObjects live;
    return live;
}

struct Object {
    explicit Object(Kind k) : kind(k) {
        pthread_mutex_lock(&Live().lock);
        Live().set.insert(this);
        pthread_mutex_unlock(&Live().lock);
    }
    ~Object() {
        pthread_mutex_lock(&Live().lock);
        Live().set.erase(this);
        pthread_mutex_unlock(&Live().lock);
    }
    Kind kind;
};

struct Synchronization {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
};

Synchronization& Sync() {
    static Synchronization sync;
    return sync;
}

void NotifyChanged() {
    pthread_mutex_lock(&Sync().mutex);
    pthread_cond_broadcast(&Sync().changed);
    pthread_mutex_unlock(&Sync().mutex);
}

struct Event : Object {
    Event(bool manual, bool initial) : Object(Kind::Event), manualReset(manual), signaled(initial) {}
    bool manualReset;
    bool signaled;
};

struct Semaphore : Object {
    Semaphore(LONG initial, LONG maximum) : Object(Kind::Semaphore), count(initial), limit(maximum) {}
    LONG count;
    LONG limit;
};

struct Thread : Object {
    Thread() : Object(Kind::Thread) {}
    pthread_t id = 0;
    LPTHREAD_START_ROUTINE start = nullptr;
    LPVOID parameter = nullptr;
    DWORD exitCode = 259;  // STILL_ACTIVE
    bool finished = false;
    bool started = false;
    DWORD tid = 0;
};

struct FindState : Object {
    FindState() : Object(Kind::Find) {}
    std::string directory;
    std::string pattern;
    DIR* dir = nullptr;
};

thread_local DWORD t_lastError = 0;
thread_local Thread* t_currentThread = nullptr;

void SetLastErrorValue(DWORD error) { t_lastError = error; }

// Small values are file descriptors plus one (src/port/native/win_compat.cpp), not objects.
constexpr uintptr_t kSmallHandleLimit = 0x10000;

Object* AsObject(HANDLE handle) {
    if (!handle || handle == INVALID_HANDLE_VALUE || reinterpret_cast<uintptr_t>(handle) < kSmallHandleLimit) return nullptr;
    pthread_mutex_lock(&Live().lock);
    const bool live = Live().set.count(handle) != 0;
    pthread_mutex_unlock(&Live().lock);
    return live ? static_cast<Object*>(handle) : nullptr;
}

bool IsSignaled(Object* object) {
    switch (object->kind) {
        case Kind::Event: return static_cast<Event*>(object)->signaled;
        case Kind::Semaphore: return static_cast<Semaphore*>(object)->count > 0;
        case Kind::Thread: return static_cast<Thread*>(object)->finished;
        default: return false;
    }
}

void ConsumeSignal(Object* object) {
    if (object->kind == Kind::Event) {
        Event* event = static_cast<Event*>(object);
        if (!event->manualReset) event->signaled = false;
    } else if (object->kind == Kind::Semaphore) {
        --static_cast<Semaphore*>(object)->count;
    }
}

timespec DeadlineAfter(DWORD milliseconds) {
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    now.tv_sec += milliseconds / 1000;
    now.tv_nsec += static_cast<long>(milliseconds % 1000) * 1000000L;
    if (now.tv_nsec >= 1000000000L) {
        now.tv_nsec -= 1000000000L;
        ++now.tv_sec;
    }
    return now;
}

DWORD WaitForAny(DWORD count, const HANDLE* handles, bool waitAll, DWORD milliseconds) {
    pthread_mutex_lock(&Sync().mutex);
    const timespec deadline = milliseconds == INFINITE ? timespec{} : DeadlineAfter(milliseconds);
    DWORD result = WAIT_FAILED;
    for (;;) {
        DWORD ready = 0;
        DWORD first = count;
        for (DWORD i = 0; i < count; ++i) {
            Object* object = AsObject(handles[i]);
            if (object && IsSignaled(object)) {
                ++ready;
                if (first == count) first = i;
            }
        }
        if (count > 0 && (waitAll ? ready == count : ready > 0)) {
            for (DWORD i = waitAll ? 0 : first; i < (waitAll ? count : first + 1); ++i) ConsumeSignal(AsObject(handles[i]));
            result = WAIT_OBJECT_0 + (waitAll ? 0 : first);
            break;
        }
        if (milliseconds == 0) {
            result = static_cast<DWORD>(WAIT_TIMEOUT);
            break;
        }
        if (milliseconds == INFINITE) {
            pthread_cond_wait(&Sync().changed, &Sync().mutex);
        } else if (pthread_cond_timedwait(&Sync().changed, &Sync().mutex, &deadline) == ETIMEDOUT) {
            result = static_cast<DWORD>(WAIT_TIMEOUT);
            break;
        }
    }
    pthread_mutex_unlock(&Sync().mutex);
    return result;
}

void* ThreadMain(void* argument) {
    Thread* thread = static_cast<Thread*>(argument);
    t_currentThread = thread;
    thread->tid = reb::platform::CurrentThreadId();
    reb::platform::SetCurrentThreadName("rebellion-worker");
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, nullptr);
    const DWORD code = thread->start(thread->parameter);
    pthread_mutex_lock(&Sync().mutex);
    thread->exitCode = code;
    thread->finished = true;
    pthread_cond_broadcast(&Sync().changed);
    pthread_mutex_unlock(&Sync().mutex);
    return nullptr;
}

bool WildcardMatch(const char* pattern, const char* text) {
    for (; *pattern; ++pattern, ++text) {
        if (*pattern == '*') {
            while (pattern[1] == '*') ++pattern;
            for (const char* t = text;; ++t) {
                if (WildcardMatch(pattern + 1, t)) return true;
                if (!*t) return false;
            }
        }
        if (!*text) return false;
        if (*pattern != '?' && tolower(static_cast<unsigned char>(*pattern)) != tolower(static_cast<unsigned char>(*text))) return false;
    }
    return !*text;
}

void FileTimeFrom(timespec t, FILETIME* out) {
    const uint64_t ticks = (static_cast<uint64_t>(t.tv_sec) + 11644473600ull) * 10000000ull + static_cast<uint64_t>(t.tv_nsec) / 100;
    out->dwLowDateTime = static_cast<DWORD>(ticks);
    out->dwHighDateTime = static_cast<DWORD>(ticks >> 32);
}

bool FillFindData(FindState* state, const dirent* entry, WIN32_FIND_DATAA* data) {
    std::string full = state->directory.empty() ? std::string(entry->d_name) : state->directory + "/" + entry->d_name;
    struct stat st;
    if (stat(full.c_str(), &st) != 0) return false;
    memset(data, 0, sizeof *data);
    data->dwFileAttributes = S_ISDIR(st.st_mode) ? 0x10 : 0x80;
    FileTimeFrom(st.st_mtim, &data->ftLastWriteTime);
    FileTimeFrom(st.st_atim, &data->ftLastAccessTime);
    FileTimeFrom(st.st_ctim, &data->ftCreationTime);
    data->nFileSizeHigh = static_cast<DWORD>(static_cast<uint64_t>(st.st_size) >> 32);
    data->nFileSizeLow = static_cast<DWORD>(st.st_size);
    strncpy(data->cFileName, entry->d_name, sizeof data->cFileName - 1);
    return true;
}

bool NextMatch(FindState* state, WIN32_FIND_DATAA* data) {
    while (const dirent* entry = readdir(state->dir))
        if (WildcardMatch(state->pattern.c_str(), entry->d_name) && FillFindData(state, entry, data)) return true;
    return false;
}

}  // namespace

extern "C" {

DWORD REB_STDCALL GetLastError(void) { return t_lastError; }

BOOL REB_STDCALL CloseHandle(HANDLE handle) {
    if (handle && handle != INVALID_HANDLE_VALUE && reinterpret_cast<uintptr_t>(handle) < kSmallHandleLimit)
        return close(static_cast<int>(reinterpret_cast<uintptr_t>(handle)) - 1) == 0;
    Object* object = AsObject(handle);
    if (!object) {
        SetLastErrorValue(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    switch (object->kind) {
        case Kind::Event: delete static_cast<Event*>(object); break;
        case Kind::Semaphore: delete static_cast<Semaphore*>(object); break;
        case Kind::Thread: {
            Thread* thread = static_cast<Thread*>(object);
            if (thread->started && !thread->finished) pthread_detach(thread->id);
            else if (thread->started) pthread_join(thread->id, nullptr);
            if (!thread->started || thread->finished) delete thread;
            break;
        }
        case Kind::Find: FindClose(handle); break;
    }
    return TRUE;
}

HANDLE REB_STDCALL CreateEventA(LPSECURITY_ATTRIBUTES, BOOL manualReset, BOOL initialState, LPCSTR) {
    SetLastErrorValue(0);
    return new Event(manualReset != 0, initialState != 0);
}

BOOL REB_STDCALL SetEvent(HANDLE handle) {
    Object* object = AsObject(handle);
    if (!object || object->kind != Kind::Event) {
        SetLastErrorValue(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    pthread_mutex_lock(&Sync().mutex);
    static_cast<Event*>(object)->signaled = true;
    pthread_cond_broadcast(&Sync().changed);
    pthread_mutex_unlock(&Sync().mutex);
    return TRUE;
}

HANDLE REB_STDCALL CreateSemaphoreA(LPSECURITY_ATTRIBUTES, LONG initialCount, LONG maximumCount, LPCSTR) {
    if (maximumCount <= 0 || initialCount < 0 || initialCount > maximumCount) {
        SetLastErrorValue(87);
        return nullptr;
    }
    SetLastErrorValue(0);
    return new Semaphore(initialCount, maximumCount);
}

DWORD REB_STDCALL WaitForSingleObject(HANDLE handle, DWORD milliseconds) {
    if (!AsObject(handle)) {
        SetLastErrorValue(ERROR_INVALID_HANDLE);
        return WAIT_FAILED;
    }
    return WaitForAny(1, &handle, false, milliseconds);
}

DWORD REB_STDCALL WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD milliseconds) {
    return WaitForAny(count, handles, waitAll != 0, milliseconds);
}

void REB_STDCALL Sleep(DWORD milliseconds) {
    timespec t = {static_cast<time_t>(milliseconds / 1000), static_cast<long>(milliseconds % 1000) * 1000000L};
    while (nanosleep(&t, &t) == -1 && errno == EINTR) {
    }
}

DWORD REB_STDCALL SleepEx(DWORD milliseconds, BOOL) {
    Sleep(milliseconds);
    return 0;
}

HANDLE REB_STDCALL CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T stackSize, LPTHREAD_START_ROUTINE start, LPVOID parameter, DWORD, LPDWORD threadId) {
    Thread* thread = new Thread;
    thread->start = start;
    thread->parameter = parameter;
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, stackSize > (256u << 10) ? stackSize : (256u << 10));
    const int rc = pthread_create(&thread->id, &attributes, ThreadMain, thread);
    pthread_attr_destroy(&attributes);
    if (rc != 0) {
        delete thread;
        SetLastErrorValue(8);
        return nullptr;
    }
    thread->started = true;
    if (threadId) *threadId = static_cast<DWORD>(reinterpret_cast<uintptr_t>(thread) >> 4);
    return thread;
}

DWORD REB_STDCALL GetCurrentThreadId(void) { return reb::platform::CurrentThreadId(); }

BOOL REB_STDCALL SetThreadPriority(HANDLE, int) { return TRUE; }

void REB_STDCALL ExitThread(DWORD exitCode) {
    if (Thread* thread = t_currentThread) {
        pthread_mutex_lock(&Sync().mutex);
        thread->exitCode = exitCode;
        thread->finished = true;
        pthread_cond_broadcast(&Sync().changed);
        pthread_mutex_unlock(&Sync().mutex);
    }
    pthread_exit(nullptr);
}

BOOL REB_STDCALL TerminateThread(HANDLE handle, DWORD exitCode) {
    Object* object = AsObject(handle);
    if (!object || object->kind != Kind::Thread) {
        SetLastErrorValue(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    Thread* thread = static_cast<Thread*>(object);
    if (thread->started && !thread->finished) {
        pthread_cancel(thread->id);
        pthread_join(thread->id, nullptr);
        pthread_mutex_lock(&Sync().mutex);
        thread->exitCode = exitCode;
        thread->finished = true;
        pthread_cond_broadcast(&Sync().changed);
        pthread_mutex_unlock(&Sync().mutex);
    }
    return TRUE;
}

void REB_STDCALL InitializeCriticalSection(LPCRITICAL_SECTION section) {
    pthread_mutex_t* mutex = new pthread_mutex_t;
    pthread_mutexattr_t attributes;
    pthread_mutexattr_init(&attributes);
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(mutex, &attributes);
    pthread_mutexattr_destroy(&attributes);
    memset(section, 0, sizeof *section);
    section->LockSemaphore = mutex;
}

void REB_STDCALL EnterCriticalSection(LPCRITICAL_SECTION section) {
    pthread_mutex_lock(static_cast<pthread_mutex_t*>(section->LockSemaphore));
}

void REB_STDCALL LeaveCriticalSection(LPCRITICAL_SECTION section) {
    pthread_mutex_unlock(static_cast<pthread_mutex_t*>(section->LockSemaphore));
}

void REB_STDCALL DeleteCriticalSection(LPCRITICAL_SECTION section) {
    pthread_mutex_t* mutex = static_cast<pthread_mutex_t*>(section->LockSemaphore);
    if (!mutex) return;
    pthread_mutex_destroy(mutex);
    delete mutex;
    section->LockSemaphore = nullptr;
}

LONG REB_STDCALL InterlockedExchange(LONG volatile* target, LONG value) { return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST); }

BOOL REB_STDCALL QueryPerformanceFrequency(LARGE_INTEGER* frequency) {
    frequency->QuadPart = native_clock::Deterministic() ? 1000000 : 1000000000;
    return TRUE;
}

BOOL REB_STDCALL QueryPerformanceCounter(LARGE_INTEGER* counter) {
    if (native_clock::Deterministic()) {
        counter->QuadPart = static_cast<LONGLONG>(native_clock::AdvanceCounterNs() / 1000);
        return TRUE;
    }
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    counter->QuadPart = static_cast<LONGLONG>(now.tv_sec) * 1000000000 + now.tv_nsec;
    return TRUE;
}

DWORD REB_STDCALL GetTickCount(void) {
    if (native_clock::Deterministic()) return native_clock::VirtualTickMs();
    return native_clock::RealTickMs();
}

void REB_STDCALL OutputDebugStringA(LPCSTR text) {
    if (text && getenv("REB_DEBUG_OUTPUT")) {
        fputs(text, stderr);
    }
}

UINT REB_STDCALL GetDriveTypeA(LPCSTR) { return DRIVE_FIXED; }

BOOL REB_STDCALL GetComputerNameA(LPSTR buffer, LPDWORD size) {
    char name[256];
    if (gethostname(name, sizeof name) != 0) strcpy(name, "REBELLION");
    name[sizeof name - 1] = 0;
    const size_t length = strlen(name);
    if (length >= *size) {
        *size = static_cast<DWORD>(length + 1);
        SetLastErrorValue(111);
        return FALSE;
    }
    memcpy(buffer, name, length + 1);
    *size = static_cast<DWORD>(length);
    return TRUE;
}

BOOL REB_STDCALL DeleteFileA(LPCSTR fileName) {
    if (unlink(mw::res::ResolvePath(fileName).c_str()) == 0) return TRUE;
    SetLastErrorValue(errno == ENOENT ? 2 : 5);
    return FALSE;
}

HANDLE REB_STDCALL FindFirstFileA(LPCSTR fileName, LPWIN32_FIND_DATAA data) {
    std::string path = mw::res::ResolvePath(fileName);
    const size_t slash = path.rfind('/');
    FindState* state = new FindState;
    state->directory = slash == std::string::npos ? std::string() : path.substr(0, slash);
    state->pattern = slash == std::string::npos ? path : path.substr(slash + 1);
    state->dir = opendir(state->directory.empty() ? "." : state->directory.c_str());
    if (!state->dir || !NextMatch(state, data)) {
        SetLastErrorValue(state->dir ? 2 : 3);
        if (state->dir) closedir(state->dir);
        delete state;
        return INVALID_HANDLE_VALUE;
    }
    return state;
}

BOOL REB_STDCALL FindClose(HANDLE handle) {
    Object* object = AsObject(handle);
    if (!object || object->kind != Kind::Find) {
        SetLastErrorValue(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    FindState* state = static_cast<FindState*>(object);
    if (state->dir) closedir(state->dir);
    delete state;
    return TRUE;
}

HFILE REB_STDCALL _lopen(LPCSTR pathName, int mode) {
    const int access = (mode & 3) == 1 ? O_WRONLY : (mode & 3) == 2 ? O_RDWR : O_RDONLY;
    const int fd = open(mw::res::ResolvePath(pathName).c_str(), access | O_CLOEXEC);
    if (fd < 0) {
        SetLastErrorValue(errno == ENOENT ? 2 : 5);
        return HFILE_ERROR;
    }
    return fd;
}

UINT REB_STDCALL _lread(HFILE file, LPVOID buffer, UINT bytes) {
    const ssize_t n = read(file, buffer, bytes);
    return n < 0 ? static_cast<UINT>(HFILE_ERROR) : static_cast<UINT>(n);
}

HFILE REB_STDCALL _lclose(HFILE file) { return close(file) == 0 ? 0 : HFILE_ERROR; }

}  // extern "C"
