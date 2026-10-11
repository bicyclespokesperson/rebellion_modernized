#include <cstring>
#include <cstdlib>
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
    VerifyFileRoundTripAndFailureState();
}
