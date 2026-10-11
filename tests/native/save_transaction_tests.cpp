#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "port/native/save_transaction.h"

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void RequireNoTemporaryFiles(const std::filesystem::path& directory) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        Require(entry.path().filename().string().find(".tmp.") == std::string::npos);
    }
}

}

int main() {
    {
        ScopedStrstream stream;
        WritePositionMarker(stream.get()->Out());
        istream_seekg_pos(stream.get()->In(), 0);
        Require(CheckPositionMarker(stream.get()->In()));
    }
    {
        ScopedStrstream stream;
        const int32_t invalidMarker = 99;
        StreamWrite32(stream.get()->Out(), &invalidMarker);
        istream_seekg_pos(stream.get()->In(), 0);
        Require(!CheckPositionMarker(stream.get()->In()));
        Require((iostream_access::State(stream.get()->In()) & iostream_flags::kFail) != 0);
    }
    {
        ScopedStrstream stream;
        if constexpr (sizeof(size_t) > sizeof(uint32_t)) {
            Require(!WriteCount32(stream.get()->Out(), static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1));
            Require((iostream_access::State(stream.get()->Out()) & iostream_flags::kFail) != 0);
            Require(iostream_access::BufferOf(stream.get())->m_pptr == nullptr);
        }
    }

    std::string pattern = (std::filesystem::temp_directory_path() / "reb-save-test-XXXXXX").string();
    Require(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path directory(pattern);
    const std::filesystem::path target = directory / "game.sav";
    std::ofstream(target, std::ios::binary) << "original";

    const auto write = [](ostream* out) {
        constexpr char bytes[] = "replacement";
        ostream_write(out, bytes, sizeof(bytes) - 1);
        return true;
    };
    Require(!reb::native::WriteSaveAtomically(target.c_str(), [&](ostream* out) {
        write(out);
        return false;
    }));
    Require(Read(target) == "original");
    RequireNoTemporaryFiles(directory);

    Require(!reb::native::WriteSaveAtomically(target.c_str(), [](ostream* out) {
        return WriteLegacyString(out, std::string(65536, 'x'));
    }));
    Require(Read(target) == "original");
    RequireNoTemporaryFiles(directory);

    Require(!reb::native::WriteSaveAtomically(target.c_str(), [](ostream*) -> bool {
        throw std::runtime_error("serialization failed");
    }));
    Require(Read(target) == "original");
    RequireNoTemporaryFiles(directory);

    Require(reb::native::WriteSaveAtomically(target.c_str(), write));
    Require(Read(target) == "replacement");
    RequireNoTemporaryFiles(directory);

    const auto directoryTarget = directory / "destination-directory";
    std::filesystem::create_directory(directoryTarget);
    Require(!reb::native::WriteSaveAtomically(directoryTarget.c_str(), write));
    Require(std::filesystem::is_directory(directoryTarget));
    RequireNoTemporaryFiles(directory);
    Require(!reb::native::WriteSaveAtomically(nullptr, write));
    std::filesystem::remove_all(directory);
}
