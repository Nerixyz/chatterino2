#include "common/QLogging.hpp"
#include "Test.hpp"

#include <vector>

#ifdef CHATTERINO_HARDENED
TEST(StlHardening, vectorSubscript)
{
    // Use lambda to avoid issues with commas in macros.
    auto fn = [] {
        std::vector<int> vec{1, 23};
        qCDebug(chatterinoApp) << vec[2];  // oops
    };
    ASSERT_DEATH({ fn(); }, "vector");
}
#endif
