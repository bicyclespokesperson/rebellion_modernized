#include <climits>
#include <cstdlib>

#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "core/runtime_environment.h"
#include "data/random.h"
#include "data/game_math.h"

RandomGenerator* g_gameRandom = nullptr;
const int g_percentMin = 0;
const int g_percentMax = 100;

int64_t GameSeedSeconds() { return 1; }

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

void Rewind(ScopedStrstream& stream) {
    Require(istream_seekg_pos(stream.get()->In(), 0) == stream.get()->In());
}

void VerifyWarmupAndDeterminism() {
    constexpr int seed = 12345;
    RandomGenerator generator(seed);
    int expectedState = seed;
    for (int i = 0; i < 16; ++i) expectedState = generator.Step(expectedState);
    Require(generator.m_seed == expectedState);

    RandomGenerator first(seed);
    RandomGenerator second(seed);
    for (int i = 0; i < 1000; ++i) Require(first.Next(37) == second.Next(37));
}

void VerifyInclusiveAndHalfOpenBounds() {
    RandomGenerator generator(7654321);
    Require(generator.Next(INT_MIN) == 0);
    Require(generator.Next(0) == 0);
    Require(generator.Next(1) == 0);
    const int stateBeforeSingleton = generator.m_seed;
    Require(generator.Between(4, 4) == 4);
    Require(generator.m_seed == stateBeforeSingleton);
    for (int i = 0; i < 2000; ++i) {
        const int value = generator.Next(17);
        Require(value >= 0 && value < 17);
        const int inclusive = generator.Between(-4, 4);
        Require(inclusive >= -4 && inclusive <= 4);
        const int reversed = generator.Between(4, -4);
        Require(reversed >= -4 && reversed <= 4);
    }
    const int fullRange = generator.Between(INT_MIN, INT_MAX);
    (void)fullRange;
}

void VerifyGameplayAndPresentationStreamsAreIndependent() {
    RandomGenerator gameplay(91);
    RandomGenerator gameplayBaseline(91);
    RandomGenerator presentation(42);
    g_gameRandom = &gameplay;

    for (int i = 0; i < 100; ++i) (void)presentation.Next(12);

    Require(GameplayRandomNext(25) == gameplayBaseline.Next(25));
    Require(GameplayRandomNext(25) == gameplayBaseline.Next(25));
    Require(GameplayRandomNext(1) == 0);
    Require(GameplayRandomNext(0) == 0);
    Require(GameRandom(INT_MAX) >= 0);
    Require(GameRandom(INT_MIN) <= 0);
    const int edgeRange = GameRandomRange(INT_MIN, INT_MAX);
    (void)edgeRange;
    g_gameRandom = nullptr;
    Require(GameplayRandomNext(25) == 0);
}

void VerifySavedStateRoundTripAndValidation() {
    RandomGenerator source(54321);
    for (int i = 0; i < 10; ++i) (void)source.Next(100);

    ScopedStrstream stream;
    source.Write(stream.get()->Out());
    Rewind(stream);

    RandomGenerator restored(9);
    restored.Read(stream.get()->In());
    for (int i = 0; i < 100; ++i) Require(source.Next(1000) == restored.Next(1000));

    ScopedStrstream invalid;
    int badSeed = 0;
    WriteValue32(invalid.get()->Out(), &badSeed);
    Rewind(invalid);
    RandomGenerator unchanged(321);
    const int oldState = unchanged.m_seed;
    unchanged.Read(invalid.get()->In());
    Require(unchanged.m_seed == oldState);
    Require((invalid.get()->Ios()->m_state & iostream_flags::kFail) != 0);
}

}  // namespace

int main() {
    VerifyWarmupAndDeterminism();
    VerifyInclusiveAndHalfOpenBounds();
    VerifyGameplayAndPresentationStreamsAreIndependent();
    VerifySavedStateRoundTripAndValidation();
}
