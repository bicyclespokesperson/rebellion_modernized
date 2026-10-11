#include <cstring>
#include <cstdlib>
#include <limits>
#include <unistd.h>

#include "core/iostream_lib.h"

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

void VerifyStringRoundTripAndFreeze() {
    constexpr char kText[] = "typed stream round trip";
    char* detached = nullptr;
    {
        ScopedStrstream stream;
        ostream_write(stream.get()->Out(), kText, sizeof(kText) - 1);
        Require(istream_seekg_pos(stream.get()->In(), 0) == stream.get()->In());
        char result[sizeof(kText)]{};
        istream_read(stream.get()->In(), result, sizeof(kText) - 1);
        Require(stream.get()->In()->m_gcount == static_cast<int>(sizeof(kText) - 1));
        Require(std::memcmp(result, kText, sizeof(kText) - 1) == 0);

        detached = strstreambuf_str(iostream_access::BufferOf(stream.get()));
        Require(detached != nullptr);
        Require(std::memcmp(detached, kText, sizeof(kText) - 1) == 0);
    }

    // str() freezes the dynamically allocated buffer: stream cleanup leaves the returned storage owned by the caller.
    Require(std::memcmp(detached, kText, sizeof(kText) - 1) == 0);
    ::operator delete(detached);
}

void VerifyGrowingSeek() {
    ScopedStrstream stream;
    ostream_write(stream.get()->Out(), "abc", 3);
    auto* buffer = iostream_access::BufferOf(stream.get());
    Require(strstreambuf_seekoff(static_cast<strstreambuf*>(buffer), 4096,
                                iostream_flags::kSeekBeg, iostream_flags::kOut) == 4096);
    ostream_write(stream.get()->Out(), "z", 1);
    Require(istream_seekg_pos(stream.get()->In(), 0) == stream.get()->In());
    char prefix[3]{};
    istream_read(stream.get()->In(), prefix, 3);
    Require(std::memcmp(prefix, "abc", 3) == 0);
    char gap[4093];
    istream_read(stream.get()->In(), gap, sizeof(gap));
    for (char byte : gap) Require(byte == 0);
    Require(istream_seekg_pos(stream.get()->In(), 4096) == stream.get()->In());
    char last = 0;
    istream_read(stream.get()->In(), &last, 1);
    Require(last == 'z');
    Require(strstreambuf_seekoff(static_cast<strstreambuf*>(buffer), -1,
                                iostream_flags::kSeekBeg, iostream_flags::kOut) == -1);
}

void VerifyZeroedInCapacityGap() {
    ScopedStrstream stream;
    auto* buffer = static_cast<strstreambuf*>(iostream_access::BufferOf(stream.get()));
    Require(strstreambuf_setbuf(buffer, nullptr, 64) == buffer);
    ostream_write(stream.get()->Out(), "abc", 3);
    Require(ostream_seekp_pos(stream.get()->Out(), 20) == stream.get()->Out());
    ostream_write(stream.get()->Out(), "Z", 1);

    Require(istream_seekg_pos(stream.get()->In(), 0) == stream.get()->In());
    char prefix[3]{};
    istream_read(stream.get()->In(), prefix, sizeof(prefix));
    Require(std::memcmp(prefix, "abc", sizeof(prefix)) == 0);
    char gap[17];
    istream_read(stream.get()->In(), gap, sizeof(gap));
    for (char byte : gap) Require(byte == 0);
    char last = 0;
    istream_read(stream.get()->In(), &last, 1);
    Require(last == 'Z');
}

void VerifyBackpatchPreservesWrittenBytes() {
    ScopedStrstream stream;
    ostream_write(stream.get()->Out(), "abcdef", 6);
    Require(ostream_seekp_pos(stream.get()->Out(), 2) == stream.get()->Out());
    ostream_write(stream.get()->Out(), "X", 1);
    Require(ostream_seekp_pos(stream.get()->Out(), 5) == stream.get()->Out());
    ostream_write(stream.get()->Out(), "z", 1);

    Require(istream_seekg_pos(stream.get()->In(), 0) == stream.get()->In());
    char result[6]{};
    istream_read(stream.get()->In(), result, sizeof(result));
    Require(std::memcmp(result, "abXdez", sizeof(result)) == 0);
}

void VerifyInvalidSeeksSetFail() {
    {
        ScopedStrstream stream;
        ostream_write(stream.get()->Out(), "abc", 3);
        istream_seekg_pos(stream.get()->In(), -1);
        Require((iostream_access::State(stream.get()) & iostream_flags::kFail) != 0);
    }
    {
        ScopedStrstream stream;
        ostream_write(stream.get()->Out(), "abc", 3);
        istream_seekg_pos(stream.get()->In(), std::numeric_limits<long>::max());
        Require((iostream_access::State(stream.get()) & iostream_flags::kFail) != 0);
    }
}

void VerifyOverlongIntegerSetsFail() {
    ScopedStrstream stream;
    ostream_write(stream.get()->Out(), "123456789012345", 15);
    Require(istream_seekg_pos(stream.get()->In(), 0) == stream.get()->In());
    char digits[16]{};
    istream_getint(stream.get()->In(), digits);
    Require((iostream_access::State(stream.get()) & iostream_flags::kFail) != 0);
}

void VerifyFileRoundTripAndFailureState() {
    char path[] = "/tmp/reb-stream-io-test-XXXXXX";
    const int temporary = mkstemp(path);
    Require(temporary != -1);
    close(temporary);
    unlink(path);

    constexpr char kText[] = "file stream round trip";
    {
        ScopedOfstream output;
        ofstream_open(output.get(), path, 0x82, 0x180);
        Require(output.get()->Ios()->m_state == 0);
        ostream_write(output.get()->Out(), kText, sizeof(kText) - 1);
        fstreambase_close(output.get()->Out());
        Require(output.get()->Ios()->m_state == 0);
    }

    {
        ScopedIfstream input;
        ifstream_open(input.get(), path, 0xa1, 0x180);
        Require(input.get()->Ios()->m_state == 0);
        char result[sizeof(kText)]{};
        istream_read(input.get()->In(), result, sizeof(kText) - 1);
        Require(input.get()->In()->m_gcount == static_cast<int>(sizeof(kText) - 1));
        Require(std::memcmp(result, kText, sizeof(kText) - 1) == 0);
        fstreambase_close(input.get()->In());
        Require(input.get()->Ios()->m_state == 0);
    }

    {
        ScopedIfstream missing;
        unlink(path);
        ifstream_open(missing.get(), path, 0xa1, 0x180);
        Require((missing.get()->Ios()->m_state & iostream_flags::kFail) != 0);
        Require(static_cast<filebuf*>(missing.get()->Ios()->m_bp)->m_fd == -1);
    }

    unlink(path);
}

}  // namespace

int main() {
    VerifyStringRoundTripAndFreeze();
    VerifyGrowingSeek();
    VerifyZeroedInCapacityGap();
    VerifyBackpatchPreservesWrittenBytes();
    VerifyInvalidSeeksSetFail();
    VerifyOverlongIntegerSetsFail();
    VerifyFileRoundTripAndFailureState();
}
