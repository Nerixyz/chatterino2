#include "snapshots/BasicFullMatch.snapshot.hpp"
#include "snapshots/BasicPartialMatch.snapshot.hpp"
#include "snapshots/UnicodeFullMatch.snapshot.hpp"
#include "snapshots/UnicodePartialMatch.snapshot.hpp"

#include <gtest/gtest.h>
#include <triegen/tree.hpp>

#include <filesystem>
#include <fstream>

namespace {

using IntNode = triegen::Node<int>;
using IntRun = triegen::Run<int>;
using IntEnd = triegen::End<int>;
using IntCases = triegen::Cases<int>;

auto make(auto &&data)
{
    return std::unique_ptr<IntNode>(
        new IntNode(std::forward<decltype(data)>(data)));
};

template <typename T>
struct CasesBuilder {
    triegen::Cases<T> inner;

    CasesBuilder<T> &&add(char16_t c, auto &&data) &&
    {
        this->inner.cases.emplace(c, make(std::forward<decltype(data)>(data)));
        return std::move(*this);
    }

    auto build() &&
    {
        return std::move(this->inner);
    }

    auto mbuild() &&
    {
        return make(std::move(this->inner));
    }
};

CasesBuilder<int> cases()
{
    return {};
}

auto run(std::u16string_view run, auto &&next)
{
    return IntRun{.run = run, .next = make(std::forward<decltype(next)>(next))};
}

auto end(int value, size_t totalLength)
{
    return IntEnd{.value = value, .totalLength = totalLength};
}

auto end(int value, size_t totalLength, auto &&next)
{
    return IntEnd{
        .value = value,
        .next = make(std::forward<decltype(next)>(next)),
        .totalLength = totalLength,
    };
}

void compareSnapshot(const char *name, std::string_view data, bool write = true)
{
    auto path =
        std::filesystem::path(__FILE__).parent_path() / "snapshots" / name;

    if (write)
    {
        std::ofstream out(path);
        out << data;
        return;
    }

    std::ifstream is(path);
    std::ostringstream buffer;
    buffer << is.rdbuf();

    ASSERT_EQ(buffer.view(), data);
}

std::pair<std::string, std::string> printToStr(
    const std::unique_ptr<IntNode> &root, triegen::PrintOptions options)
{
    std::ostringstream srcBuf;
    triegen::printTree(srcBuf, root, options);

    std::ostringstream headerBuf;
    triegen::printHeader<int>(headerBuf, options);
    return {srcBuf.str(), headerBuf.str()};
}

}  // namespace

TEST(triegenTree, insert)
{
    auto root = triegen::makeTree<int>();
    ASSERT_EQ(root, nullptr);

    // init
    triegen::appendValue(root, u"foobar", 1);
    ASSERT_EQ(*root, *make(run(u"foobar", end(1, 6))));

    // run -> cases
    triegen::appendValue(root, u"barfoo", 2);
    ASSERT_EQ(*root, *cases()
                          .add(u'b', run(u"arfoo", end(2, 6)))
                          .add(u'f', run(u"oobar", end(1, 6)))
                          .mbuild());

    // run -> cases
    triegen::appendValue(root, u"baz", 3);
    ASSERT_EQ(*root,
              *cases()
                   .add(u'b', run(u"a", cases()
                                            .add(u'r', run(u"foo", end(2, 6)))
                                            .add(u'z', end(3, 3))
                                            .build()))
                   .add(u'f', run(u"oobar", end(1, 6)))
                   .mbuild());

    // run -> [end, run]
    triegen::appendValue(root, u"foo", 4);
    ASSERT_EQ(*root,
              *cases()
                   .add(u'b', run(u"a", cases()
                                            .add(u'r', run(u"foo", end(2, 6)))
                                            .add(u'z', end(3, 3))
                                            .build()))
                   .add(u'f', run(u"oo", end(4, 3, run(u"bar", end(1, 6)))))
                   .mbuild());

    // add case
    triegen::appendValue(root, u"bam", 5);
    ASSERT_EQ(*root,
              *cases()
                   .add(u'b', run(u"a", cases()
                                            .add(u'r', run(u"foo", end(2, 6)))
                                            .add(u'z', end(3, 3))
                                            .add(u'm', end(5, 3))
                                            .build()))
                   .add(u'f', run(u"oo", end(4, 3, run(u"bar", end(1, 6)))))
                   .mbuild());

    // add to end
    triegen::appendValue(root, u"bazfoo", 6);
    ASSERT_EQ(
        *root,
        *cases()
             .add(u'b',
                  run(u"a", cases()
                                .add(u'r', run(u"foo", end(2, 6)))
                                .add(u'z', end(3, 3, run(u"foo", end(6, 6))))
                                .add(u'm', end(5, 3))
                                .build()))
             .add(u'f', run(u"oo", end(4, 3, run(u"bar", end(1, 6)))))
             .mbuild());

    // add duplicate
    triegen::appendValue(root, u"bazfoo", 6);
    ASSERT_EQ(
        *root,
        *cases()
             .add(u'b',
                  run(u"a", cases()
                                .add(u'r', run(u"foo", end(2, 6)))
                                .add(u'z', end(3, 3, run(u"foo", end(6, 6))))
                                .add(u'm', end(5, 3))
                                .build()))
             .add(u'f', run(u"oo", end(4, 3, run(u"bar", end(1, 6)))))
             .mbuild());

    // replace run with [end, run]
    triegen::appendValue(root, u"bar", 8);
    ASSERT_EQ(
        *root,
        *cases()
             .add(u'b',
                  run(u"a", cases()
                                .add(u'r', end(8, 3, run(u"foo", end(2, 6))))
                                .add(u'z', end(3, 3, run(u"foo", end(6, 6))))
                                .add(u'm', end(5, 3))
                                .build()))
             .add(u'f', run(u"oo", end(4, 3, run(u"bar", end(1, 6)))))
             .mbuild());

    // replace cases with [end, cases]
    triegen::appendValue(root, u"ba", 9);
    ASSERT_EQ(
        *root,
        *cases()
             .add(
                 u'b',
                 run(u"a", end(9, 2,
                               cases()
                                   .add(u'r', end(8, 3, run(u"foo", end(2, 6))))
                                   .add(u'z', end(3, 3, run(u"foo", end(6, 6))))
                                   .add(u'm', end(5, 3))
                                   .build())))
             .add(u'f', run(u"oo", end(4, 3, run(u"bar", end(1, 6)))))
             .mbuild());

    // add case with rest (run & end)
    triegen::appendValue(root, u"qo", 10);
    ASSERT_EQ(
        *root,
        *cases()
             .add(
                 u'b',
                 run(u"a", end(9, 2,
                               cases()
                                   .add(u'r', end(8, 3, run(u"foo", end(2, 6))))
                                   .add(u'z', end(3, 3, run(u"foo", end(6, 6))))
                                   .add(u'm', end(5, 3))
                                   .build())))
             .add(u'f', run(u"oo", end(4, 3, run(u"bar", end(1, 6)))))
             .add(u'q', run(u"o", end(10, 2)))
             .mbuild());

    // run -> [cases, end*2]
    triegen::appendValue(root, u"qu", 11);
    ASSERT_EQ(
        *root,
        *cases()
             .add(
                 u'b',
                 run(u"a", end(9, 2,
                               cases()
                                   .add(u'r', end(8, 3, run(u"foo", end(2, 6))))
                                   .add(u'z', end(3, 3, run(u"foo", end(6, 6))))
                                   .add(u'm', end(5, 3))
                                   .build())))
             .add(u'f', run(u"oo", end(4, 3, run(u"bar", end(1, 6)))))
             .add(u'q',
                  cases().add(u'o', end(10, 2)).add(u'u', end(11, 2)).build())
             .mbuild());
}

TEST(triegenTree, print)
{
    auto root = triegen::makeTree<int>();
    ASSERT_EQ(root, nullptr);

    triegen::appendValue(root, u"foobar", 1);
    triegen::appendValue(root, u"barfoo", 2);
    triegen::appendValue(root, u"baz", 3);
    triegen::appendValue(root, u"foo", 4);
    triegen::appendValue(root, u"bam", 5);
    triegen::appendValue(root, u"bazfoo", 6);
    triegen::appendValue(root, u"bar", 8);
    triegen::appendValue(root, u"ba", 9);
    triegen::appendValue(root, u"qo", 10);
    triegen::appendValue(root, u"qu", 11);

    auto fullMatch = printToStr(root, {
                                          .fnName = "tstBasicFullMatch",
                                          .fullMatch = true,
                                      });
    auto partialMatch = printToStr(root, {
                                             .fnName = "tstBasicPartialMatch",
                                             .fullMatch = false,
                                         });
    compareSnapshot("BasicFullMatch.snapshot.cpp", fullMatch.first);
    compareSnapshot("BasicFullMatch.snapshot.hpp", fullMatch.second);
    compareSnapshot("BasicPartialMatch.snapshot.cpp", partialMatch.first);
    compareSnapshot("BasicPartialMatch.snapshot.hpp", partialMatch.second);
}

TEST(triegenGen, basicFullMatch)
{
    auto m = [](QStringView v) {
        return triegen::tstBasicFullMatch(v);
    };

    ASSERT_EQ(m(u"foobar"), 1);
    ASSERT_EQ(m(u"barfoo"), 2);
    ASSERT_EQ(m(u"baz"), 3);
    ASSERT_EQ(m(u"foo"), 4);
    ASSERT_EQ(m(u"bam"), 5);
    ASSERT_EQ(m(u"bazfoo"), 6);
    ASSERT_EQ(m(u"bar"), 8);
    ASSERT_EQ(m(u"ba"), 9);
    ASSERT_EQ(m(u"qo"), 10);
    ASSERT_EQ(m(u"qu"), 11);

    ASSERT_EQ(m(u"foobar2"), -1);
    ASSERT_EQ(m(u"barfoo2"), -1);
    ASSERT_EQ(m(u"baz2"), -1);
    ASSERT_EQ(m(u"foo2"), -1);
    ASSERT_EQ(m(u"bam2"), -1);
    ASSERT_EQ(m(u"bazfoo2"), -1);
    ASSERT_EQ(m(u"bar2"), -1);
    ASSERT_EQ(m(u"ba2"), -1);
    ASSERT_EQ(m(u"qo2"), -1);
    ASSERT_EQ(m(u"qu2"), -1);

    ASSERT_EQ(m(u"foob"), -1);
    ASSERT_EQ(m(u"fooba"), -1);
    ASSERT_EQ(m(u"b"), -1);
    ASSERT_EQ(m(u"barf"), -1);
    ASSERT_EQ(m(u"qoo"), -1);
}

TEST(triegenGen, basicPartialMatch)
{
    auto m = [](QStringView v, int expectedValue, size_t expectedLen) {
        auto [value, len] = triegen::tstBasicPartialMatch(v);
        ASSERT_EQ(value, expectedValue);
        ASSERT_EQ(len, expectedLen);
    };

    m(u"foobar", 1, 6);
    m(u"barfoo", 2, 6);
    m(u"baz", 3, 3);
    m(u"foo", 4, 3);
    m(u"bam", 5, 3);
    m(u"bazfoo", 6, 6);
    m(u"bar", 8, 3);
    m(u"ba", 9, 2);
    m(u"qo", 10, 2);
    m(u"qu", 11, 2);

    m(u"foobar2", 1, 6);
    m(u"barfoo2", 2, 6);
    m(u"baz2", 3, 3);
    m(u"foo2", 4, 3);
    m(u"bam2", 5, 3);
    m(u"bazfoo2", 6, 6);
    m(u"bar2", 8, 3);
    m(u"ba2", 9, 2);
    m(u"qo2", 10, 2);
    m(u"qu2", 11, 2);

    m(u"foob", 4, 3);
    m(u"fooba", 4, 3);
    m(u"b", -1, 0);
    m(u"fa", -1, 0);
    m(u"qa", -1, 0);
    m(u"barf", 8, 3);
    m(u"qoo", 10, 2);
}

TEST(triegenTree, printUnicode)
{
    auto root = triegen::makeTree<int>();
    ASSERT_EQ(root, nullptr);

    // person bouncing ball
    triegen::appendValue(root, u"\u26F9\uFE0F", 1);
    triegen::appendValue(root, u"\u26F9", 2);
    // man bouncing ball
    triegen::appendValue(root, u"\u26F9\uFE0F\u200D\u2642\uFE0F", 3);
    triegen::appendValue(root, u"\u26F9\u200D\u2642", 4);
    // woman bouncing ball
    triegen::appendValue(root, u"\u26F9\uFE0F\u200D\u2640\uFE0F", 5);
    triegen::appendValue(root, u"\u26F9\u200D\u2640", 6);

    auto fullMatch = printToStr(root, {
                                          .fnName = "tstUnicodeFullMatch",
                                          .fullMatch = true,
                                      });
    auto partialMatch = printToStr(root, {
                                             .fnName = "tstUnicodePartialMatch",
                                             .fullMatch = false,
                                         });

    compareSnapshot("UnicodeFullMatch.snapshot.cpp", fullMatch.first);
    compareSnapshot("UnicodeFullMatch.snapshot.hpp", fullMatch.second);
    compareSnapshot("UnicodePartialMatch.snapshot.cpp", partialMatch.first);
    compareSnapshot("UnicodePartialMatch.snapshot.hpp", partialMatch.second);
}

TEST(triegenGen, unicodeFullMatch)
{
    auto m = [](QStringView v) {
        return triegen::tstUnicodeFullMatch(v);
    };

    ASSERT_EQ(m(u"\u26F9\uFE0F"), 1);
    ASSERT_EQ(m(u"\u26F9"), 2);
    ASSERT_EQ(m(u"\u26F9\uFE0F\u200D\u2642\uFE0F"), 3);
    ASSERT_EQ(m(u"\u26F9\u200D\u2642"), 4);
    ASSERT_EQ(m(u"\u26F9\uFE0F\u200D\u2640\uFE0F"), 5);
    ASSERT_EQ(m(u"\u26F9\u200D\u2640"), 6);

    ASSERT_EQ(m(u"\u26F9\uFE0F\u200D"), -1);
    ASSERT_EQ(m(u"\u26F9\u200D"), -1);
    ASSERT_EQ(m(u"\u26F9\uFE0F\u200D\u2642\uFE0F\u200D"), -1);
    ASSERT_EQ(m(u"\u26F9\u200D\u2642\uFE0F"), -1);
    ASSERT_EQ(m(u"\u26F9\uFE0F\u200D\u2640\uFE0F\u200D"), -1);
    ASSERT_EQ(m(u"\u26F9\u200D\u2640\uFE0F"), -1);
    ASSERT_EQ(m(u"\u26F8"), -1);
}

TEST(triegenGen, unicodePartialMatch)
{
    auto m = [](QStringView v, int expectedValue, size_t expectedLen) {
        auto [value, len] = triegen::tstUnicodePartialMatch(v);
        ASSERT_EQ(value, expectedValue);
        ASSERT_EQ(len, expectedLen);
    };

    m(u"\u26F9\uFE0F", 1, 2);
    m(u"\u26F9", 2, 1);
    m(u"\u26F9\uFE0F\u200D\u2642\uFE0F", 3, 5);
    m(u"\u26F9\u200D\u2642", 4, 3);
    m(u"\u26F9\uFE0F\u200D\u2640\uFE0F", 5, 5);
    m(u"\u26F9\u200D\u2640", 6, 3);

    m(u"\u26F9\uFE0F\u200D", 1, 2);
    m(u"\u26F9\u200D", 2, 1);
    m(u"\u26F9\uFE0F\u200D\u2642\uFE0F\u200D", 3, 5);
    m(u"\u26F9\uFE0F\u200D\u2642", 1, 2);
    m(u"\u26F9\u200D\u2642\uFE0F", 4, 3);
    m(u"\u26F9\uFE0F\u200D\u2640\uFE0F\u200D", 5, 5);
    m(u"\u26F9\uFE0F\u200D\u2640", 1, 2);
    m(u"\u26F9\u200D\u2640\uFE0F", 6, 3);
    m(u"\u26F8", -1, 0);
}
