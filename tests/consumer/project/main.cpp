#include <qpb/qpb.h>

#include <cstdio>
#include <cstring>

static_assert(QPB_VERSION >= QPB_VERSION_CHECK(0, 0, 1), "qpb version macros are broken");

#if CONSUMER_CXX_STANDARD >= 20
static_assert(__cplusplus >= 202002L, "consumer should be compiled as C++20");
#endif

int main()
{
    if (std::strcmp(qpb::version(), QPB_VERSION_STR) != 0) {
        std::fprintf(stderr, "qpb header/library version mismatch: headers %s, library %s\n",
            QPB_VERSION_STR, qpb::version());
        return 1;
    }
    std::printf("qpb %s (C++%d consumer)\n", qpb::version(), CONSUMER_CXX_STANDARD);
    return 0;
}
