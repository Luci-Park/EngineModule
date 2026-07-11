#pragma once
#include <engine/core/log/Assert.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace engine
{
    struct TypeId
    {
        uint32_t m_seq;          // id for runtime indexing
        uint32_t m_hash;         // id for serialization(FNV-1a of trimmed name)
        std::string_view m_name; // for logs/editor

        // if hash + name equal, seq is guaranteed
        constexpr bool operator==(const TypeId &other) const
        {
            return m_hash == other.m_hash && m_name == other.m_name;
        }
    };

    // Fowler-Noll-Vo -> string hashing function
    constexpr uint32_t Fnv1a32(std::string_view s)
    {
        uint32_t hash = 2166136261u; // offset basis
        for (unsigned char c : s)
        {
            hash ^= c;
            hash *= 16777619u; // FNV prime
        }
        return hash;
    }

    inline uint32_t NextSeq()
    {
        static std::atomic<uint32_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    namespace detail
    {
        // convert T(raw compiler-signature) to string
        template <typename T>
        constexpr std::string_view TrimTypeNameRaw()
        {
#if defined(_MSC_VER)
            // __FUNCSIG__ = std::string_view __cdecl engine::detail::TrimTypeNameRaw<struct T>(void)
            std::string_view sig = __FUNCSIG__;
            std::string_view marker = "TrimTypeNameRaw<";
#elif defined(__clang__) || defined(__GNUC__)
            // __PRETTY_FUNCTION__ = constexpr std::string_view engine::detail::TrimTypeNameRaw() [with T = {T}; std::string_view = std::basic_string_view<char>]
            std::string_view sig = __PRETTY_FUNCTION__;
            std::string_view marker = "T = ";
#else
#error "TrimTypeNameRaw: unsupported compiler -- no known signature macro, add a branch"
#endif
            const std::size_t markerPos = sig.find(marker);
            if (markerPos == std::string_view::npos)
                throw "TrimTypeNameRaw: signature marker not found";

            const std::size_t start = markerPos + marker.size();
            // slice string from the marker
#if defined(_MSC_VER)
            int depth = 1;
            std::size_t i = start;
            for (; i < sig.size(); ++i)
            {
                if (sig[i] == '<')
                    ++depth;
                else if (sig[i] == '>' && --depth == 0)
                    break;
            }
            return sig.substr(start, i - start);
#else
            const std::size_t end = sig.find_first_of(";]", start);
            return sig.substr(start, end - start);
#endif
        }

        constexpr bool IsKeywordBoundary(char prev)
        {
            return prev == '\0' || prev == '<' || prev == ',' || prev == ' ' || prev == '(';
        }

        // identifier char -- a space between two of these is significant ("unsigned int")
        constexpr bool IsWordChar(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_';
        }

        constexpr std::size_t KeywordLength(std::string_view rest)
        {
            constexpr std::string_view STRUCT_PREFIX = "struct ";
            constexpr std::string_view CLASS_PREFIX = "class ";
            constexpr std::string_view ENUM_PREFIX = "enum ";
            if (rest.substr(0, STRUCT_PREFIX.size()) == STRUCT_PREFIX)
                return STRUCT_PREFIX.size();
            if (rest.substr(0, CLASS_PREFIX.size()) == CLASS_PREFIX)
                return CLASS_PREFIX.size();
            if (rest.substr(0, ENUM_PREFIX.size()) == ENUM_PREFIX)
                return ENUM_PREFIX.size();
            return 0;
        }

        // normalize the raw signigure slice to just type name
        // ex) "struct Foo< struct Bar , struct Baz >" -> "Foo<Bar,Baz>"
        constexpr std::size_t NormalizeTypeName(std::string_view raw, char *out)
        {
            // strip trailing spaces
            while (!raw.empty() && raw.back() == ' ')
                raw.remove_suffix(1);

            std::size_t n = 0;
            char prev = '\0'; // last emitted char
            for (std::size_t i = 0; i < raw.size(); ++i)
            {
                // can it be a start of a keyword
                if (IsKeywordBoundary(prev))
                {
                    // skip if it starts with struct/class/enum
                    const std::size_t kw = KeywordLength(raw.substr(i));
                    if (kw != 0)
                    {
                        i += kw - 1; // -1 = to not skip the trailing space after keyword
                        continue;
                    }
                }
                const char c = raw[i];
                // a space is only significant between two word chars ("unsigned int");
                // any space touching punctuation (< > , * ...) is formatting noise -> drop,
                // so MSVC/GCC/Clang spacing converges to one canonical form
                if (c == ' ')
                {
                    const bool nextWord = (i + 1 < raw.size()) && IsWordChar(raw[i + 1]);
                    if (!(IsWordChar(prev) && nextWord))
                        continue;
                }
                if (out != nullptr)
                    out[n] = c;
                ++n;
                prev = c;
            }
            return n;
        }

        // offset of the bare name: past the last "::" at angle-bracket depth 0
        // (a "::" inside template args belongs to an argument, not to the outer type)
        constexpr std::size_t BareNameOffset(std::string_view normalized)
        {
            std::size_t offset = 0;
            int depth = 0;
            for (std::size_t i = 0; i + 1 < normalized.size(); ++i)
            {
                if (normalized[i] == '<')
                    ++depth;
                else if (normalized[i] == '>')
                    --depth;
                else if (depth == 0 && normalized[i] == ':' && normalized[i + 1] == ':')
                    offset = i + 2;
            }
            return offset;
        }

        // per-type static storage for the normalized name (string_views point into this)
        template <typename T>
        struct TypeNameOf
        {
            static constexpr std::size_t LENGTH = NormalizeTypeName(TrimTypeNameRaw<T>(), nullptr);

            static constexpr std::array<char, LENGTH + 1> STORAGE = []
            {
                std::array<char, LENGTH + 1> buffer{};
                NormalizeTypeName(TrimTypeNameRaw<T>(), buffer.data());
                return buffer;
            }();

            static constexpr std::string_view QUALIFIED{STORAGE.data(), LENGTH};
            static constexpr std::string_view VALUE = QUALIFIED.substr(BareNameOffset(QUALIFIED));
        };

        inline void CheckHashCollision([[maybe_unused]] uint32_t hash, [[maybe_unused]] std::string_view rawSignature)
        {
#ifndef NDEBUG
            static std::unordered_map<uint32_t, std::string_view> registry;
            auto it = registry.find(hash);
            if (it == registry.end())
            {
                registry.emplace(hash, rawSignature);
            }
            else
            {
                ENGINE_ASSERT(it->second == rawSignature,
                              "TypeId collision: \"{}\" and \"{}\" produce the same name hash {} -- "
                              "type names must be globally unique across namespaces",
                              it->second, rawSignature, hash);
            }
#endif
        }
    }

    template <typename T>
    constexpr std::string_view TrimTypeName()
    {
        return detail::TypeNameOf<T>::VALUE;
    }

    template <typename T>
    TypeId TypeIdOf()
    {
        static const TypeId id = []
        {
            constexpr std::string_view name = TrimTypeName<T>();
            constexpr uint32_t hash = Fnv1a32(name);
            detail::CheckHashCollision(hash, detail::TrimTypeNameRaw<T>());
            return TypeId{NextSeq(), hash, name};
        }();
        return id;
    }
}
