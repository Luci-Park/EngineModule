#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/TypeId.hpp>

using namespace engine;

namespace typeid_test_ns
{
    struct Namespaced
    {
    };

    struct Outer
    {
        struct Inner
        {
        };
    };

    template<typename T>
    struct Tpl
    {
    };
}

struct PlainStruct
{
};

template<typename T>
struct Wrapper
{
};

template<typename A, typename B>
struct Pair
{
};

// compile-time checks: Fnv1a32 and TrimTypeName must be constexpr-evaluable
static_assert(Fnv1a32("abc") == 0x1a47e90bu);
static_assert(Fnv1a32("abc") != Fnv1a32("abd"));
static_assert(TrimTypeName<PlainStruct>() == "PlainStruct");
static_assert(TrimTypeName<typeid_test_ns::Namespaced>() == "Namespaced");
static_assert(TrimTypeName<typeid_test_ns::Outer::Inner>() == "Inner");
// template types: normalized to compiler-independent form (no defaulted-args types here --
// those stay per-compiler; see NormalizeTypeName). '::' inside args must not split the name.
static_assert(TrimTypeName<Wrapper<PlainStruct>>() == "Wrapper<PlainStruct>");
static_assert(TrimTypeName<typeid_test_ns::Tpl<typeid_test_ns::Namespaced>>()
              == "Tpl<typeid_test_ns::Namespaced>");
static_assert(TrimTypeName<Wrapper<typeid_test_ns::Tpl<PlainStruct>>>()
              == "Wrapper<typeid_test_ns::Tpl<PlainStruct>>");

// NormalizeTypeName whitespace contract, enforced on whichever compiler builds these tests
// (MSVC now, GCC/Clang on the linux presets). If either compiler's raw signature spacing
// stops normalizing to the canonical form, the build fails here rather than silently drifting.
// NB: only types spelled identically across compilers are valid here -- MSVC renames some
// builtins (long long -> __int64) just like it prints defaulted template args, so those are
// per-compiler and deliberately excluded (see ENGINE_DESIGN.md TypeId Name-trimming caveat).
// -- multi-token builtin: the space between two word chars is LOAD-BEARING, must survive:
static_assert(TrimTypeName<unsigned int>() == "unsigned int");
// -- comma spacing: MSVC emits "A,B", GCC/Clang "A, B" -> both canonicalize to "A,B";
//    inner-arg namespace qualifiers are preserved (only the OUTER type's leading ns is stripped):
static_assert(TrimTypeName<Pair<PlainStruct, typeid_test_ns::Namespaced>>()
              == "Pair<PlainStruct,typeid_test_ns::Namespaced>");
// -- both rules at once: drop the comma space, keep the load-bearing "unsigned int" space:
static_assert(TrimTypeName<Pair<unsigned int, PlainStruct>>()
              == "Pair<unsigned int,PlainStruct>");

TEST_CASE("Fnv1a32 matches known precomputed FNV-1a 32-bit vectors", "[core][ecs][typeid]")
{
    // published FNV-1a 32-bit test vectors, computed independently of this implementation
    REQUIRE(Fnv1a32("") == 0x811c9dc5u);
    REQUIRE(Fnv1a32("a") == 0xe40c292cu);
    REQUIRE(Fnv1a32("abc") == 0x1a47e90bu);
    REQUIRE(Fnv1a32("foobar") == 0xbf9cf968u);
}

// TrimTypeName coverage lives in the static_assert block above -- a runtime TEST_CASE
// re-asserting the same constexpr equalities can never fail if this file compiles.

TEST_CASE("TypeIdOf is stable across calls for the same type", "[core][ecs][typeid]")
{
    TypeId a = TypeIdOf<PlainStruct>();
    TypeId b = TypeIdOf<PlainStruct>();
    REQUIRE(a.m_seq == b.m_seq);
    REQUIRE(a.m_hash == b.m_hash);
    REQUIRE(a.m_name == b.m_name);
    REQUIRE(a == b);
}

TEST_CASE("TypeIdOf gives distinct seq for distinct types", "[core][ecs][typeid]")
{
    TypeId a = TypeIdOf<PlainStruct>();
    TypeId b = TypeIdOf<typeid_test_ns::Namespaced>();
    REQUIRE(a.m_seq != b.m_seq);
    REQUIRE(a.m_name != b.m_name);
}

TEST_CASE("NextSeq hands out unique increasing-domain values", "[core][ecs][typeid]")
{
    uint32_t a = NextSeq();
    uint32_t b = NextSeq();
    REQUIRE(a != b);
}
