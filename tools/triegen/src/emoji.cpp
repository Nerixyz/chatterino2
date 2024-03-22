#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <triegen/tree.hpp>
#include <triegen/unicode.hpp>

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::u32string parseLiteral(std::string &&lit)
{
    std::u32string result;
    result.reserve((lit.length() + 1) / 5);
    std::istringstream is(std::move(lit));
    is >> std::hex;

    uint32_t c = 0;
    char dummy = '-';
    while (!is.eof())
    {
        is >> c;
        is >> dummy;
        result.append(1, c);
    }
    return result;
}

auto parseJson(auto path)
{
    std::ifstream is(path);
    std::ostringstream buffer;
    buffer << is.rdbuf();

    rapidjson::Document document;
    document.Parse(buffer.view().data(), buffer.view().length());
    return document;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <emoji-json> <output-cpp> <output-hpp>\n";
        return 1;
    }
    try
    {
        auto json = parseJson(argv[1]);
        assert(json.IsArray());
        auto arr = json.GetArray();
        std::vector<std::pair<std::u16string, qsizetype>> entries;
        auto putString = [&](const auto &el, const char *key, qsizetype i) {
            auto it = el.FindMember(key);
            if (it != el.MemberEnd() && it->value.IsString())
            {
                entries.emplace_back(triegen::toUtf16(parseLiteral({
                                         it->value.GetString(),
                                         it->value.GetStringLength(),
                                     })),
                                     i);
            }
        };

        qsizetype i = 0;
        for (const auto &val : arr)
        {
            auto el = val.GetObject();
            putString(el, "unified", i);
            putString(el, "non_qualified", i);
            i++;

            if (el.HasMember("skin_variations"))
            {
                for (const auto &variation : el["skin_variations"].GetObject())
                {
                    putString(variation.value, "unified", i);
                    putString(variation.value, "non_qualified", i);
                    i++;
                }
            }
        }

        auto root = triegen::makeTree<qsizetype>();
        for (const auto &entry : entries)
        {
            triegen::appendValue(root, entry.first, entry.second);
        }
        triegen::PrintOptions options{.fnName = "matchEmoji",
                                      .fullMatch = false};
        {
            std::ofstream of(argv[2], std::ios::out | std::ios::trunc);
            triegen::printTree<qsizetype>(of, root, options);
        }

        {
            std::ofstream of(argv[3], std::ios::out | std::ios::trunc);
            triegen::printHeader<qsizetype>(of, options);
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    return 0;
}
