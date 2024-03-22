#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <triegen/tree.hpp>
#include <triegen/unicode.hpp>

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std::string_view_literals;

/// Simplified QSize
struct MockQSize {
    int width;
    int height;

    auto operator<=>(const MockQSize &) const = default;

    friend std::ostream &operator<<(std::ostream &os, const MockQSize &s)
    {
        os << "{" << s.width << "," << s.height << "}";
        return os;
    }
};

namespace triegen::value {

template <>
MockQSize invalidValue()
{
    return {-1, -1};
}

template <>
std::string_view valueName<MockQSize>()
{
    return "QSize"sv;
}

template <>
std::string_view additionalIncludes<MockQSize>()
{
    return "#include <QSize>\n"sv;
}

}  // namespace triegen::value

template <typename T>
T parseValue(std::string_view v);

template <>
int parseValue(std::string_view v)
{
    std::string s(v);
    size_t idx = 0;
    try
    {
        int parsed = std::stoi(s, &idx);
        if (idx != v.length())
        {
            triegen::panic([&](std::ostream &os) {
                os << "Failed to parse integer " << v;
            });
        }
        return parsed;
    }
    catch (const std::exception &ex)
    {
        triegen::panic([&](std::ostream &os) {
            os << "Failed to parse integer " << v << ": " << ex.what();
        });
    }
    return -1;
}

template <>
MockQSize parseValue(std::string_view v)
{
    auto idx = v.find('x');
    if (idx == std::string_view::npos)
    {
        triegen::panic([&](std::ostream &os) {
            os << "Bad QSize: " << v;
        });
    }

    return {
        .width = parseValue<int>(v.substr(0, idx)),
        .height = parseValue<int>(v.substr(idx + 1)),
    };
}

template <typename T>
void processValues(const char *outFile, std::string_view functionName,
                   char **args, size_t nArgs)
{
    auto root = triegen::makeTree<T>();
    std::vector<std::pair<std::u16string, T>> items;
    items.reserve(nArgs);

    for (size_t i = 0; i < nArgs; i++)
    {
        std::string_view arg = args[i];
        auto idx = arg.find('=');
        if (idx == std::string_view::npos)
        {
            triegen::panic([&](std::ostream &os) {
                os << "Bad argument: " << arg;
            });
        }
        items.emplace_back(triegen::toUtf16(arg.substr(0, idx)),
                           parseValue<T>(arg.substr(idx + 1)));
    }

    for (const auto &[key, value] : items)
    {
        triegen::appendValue(root, key, value);
    }

    std::ofstream of(outFile, std::ios::out | std::ios::trunc);
    triegen::printTree(of, root, {.fnName = functionName, .fullMatch = true});
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <output> <function-name> [-qsize|-int] <key=value...>\n";
        return 1;
    }

    auto *out = argv[1];
    auto *fn = argv[2];
    auto *ty = argv[3];

    if (ty[0] != '-')
    {
        processValues<int>(out, fn, argv + 3, argc - 3);
        return 0;
    }
    if (ty == "-int"sv)
    {
        processValues<int>(out, fn, argv + 4, argc - 4);
        return 0;
    }
    if (ty == "-qsize"sv)
    {
        processValues<MockQSize>(out, fn, argv + 4, argc - 4);
        return 0;
    }

    return 0;
}
