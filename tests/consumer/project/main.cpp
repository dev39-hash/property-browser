#include <qpb/qpb.h>

#include <cstdio>
#include <cstring>

static_assert(QPB_VERSION >= QPB_VERSION_CHECK(0, 0, 1), "qpb version macros are broken");

#if CONSUMER_CXX_STANDARD >= 20
static_assert(__cplusplus >= 202002L, "consumer should be compiled as C++20");
#endif

namespace {

int fail(const char* message)
{
    std::fprintf(stderr, "qpb consumer: %s\n", message);
    return 1;
}

} // namespace

int main()
{
    if (std::strcmp(qpb::version(), QPB_VERSION_STR) != 0)
        return fail("header/library version mismatch");

    // Built-in types must survive static linking (no static initializers).
    for (const auto type : {qpb::Types::Bool, qpb::Types::Int, qpb::Types::Double,
             qpb::Types::String, qpb::Types::Enum, qpb::Types::FilePath, qpb::Types::DirPath}) {
        if (!qpb::TypeRegistry::global().contains(type))
            return fail("a built-in type is missing");
    }

    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("fov"), 60).range(10, 170);
    qpb::PropertyModel model(std::move(root));
    int changes = 0;
    QObject::connect(&model, &qpb::PropertyModel::valueChanged, [&changes] { ++changes; });
    if (!model.setValue(QStringLiteral("fov"), 500) || changes != 1
        || model.find(QStringLiteral("fov"))->value().toInt() != 170) {
        return fail("the value pipeline does not work");
    }

    std::printf("qpb %s (C++%d consumer)\n", qpb::version(), CONSUMER_CXX_STANDARD);
    return 0;
}
