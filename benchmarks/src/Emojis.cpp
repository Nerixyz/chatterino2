#include "providers/emoji/Emojis.hpp"

#include "common/Literals.hpp"

#include <benchmark/benchmark.h>
#include <QDebug>
#include <QString>

using namespace chatterino;
using namespace literals;

static void BM_ShortcodeParsing(benchmark::State &state)
{
    Emojis emojis;

    emojis.load();

    struct TestCase {
        QString input;
        QString expectedOutput;
    };

    std::vector<TestCase> tests{
        {
            // input
            "foo :penguin: bar",
            // expected output
            "foo 🐧 bar",
        },
        {
            // input
            "foo :nonexistantcode: bar",
            // expected output
            "foo :nonexistantcode: bar",
        },
        {
            // input
            ":male-doctor:",
            // expected output
            "👨‍⚕️",
        },
    };

    for (auto _ : state)
    {
        for (const auto &test : tests)
        {
            auto output = emojis.replaceShortCodes(test.input);

            auto matches = output == test.expectedOutput;
            if (!matches && !output.endsWith(QChar(0xFE0F)))
            {
                // Try to append 0xFE0F if needed
                output = output.append(QChar(0xFE0F));
            }
        }
    }
}

BENCHMARK(BM_ShortcodeParsing);

static void BM_EmojiParsing(benchmark::State &state, const QString &input)
{
    Emojis emojis;

    emojis.load();

    for (auto _ : state)
    {
        auto output = emojis.parse(input);
        benchmark::DoNotOptimize(output);
    }
}

BENCHMARK_CAPTURE(BM_EmojiParsing, no_emoji, u"foo bar"_s);
BENCHMARK_CAPTURE(
    BM_EmojiParsing, no_emoji_long,
    u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."_s);
BENCHMARK_CAPTURE(BM_EmojiParsing, one_emoji, u"foo 🐧 bar"_s);
BENCHMARK_CAPTURE(BM_EmojiParsing, two_emoji, u"foo 🐧 bar 🐧"_s);
BENCHMARK_CAPTURE(
    BM_EmojiParsing, repeated_emoji,
    u"😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 "_s
    "😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 "
    "😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 😂 ");
BENCHMARK_CAPTURE(
    BM_EmojiParsing, random_emoji,
    u"💲 🔔 💋 🍟 🎱 📍 🙌 🙋 🍢 😽 🙋 🙅 👧 ❇️ 💠 🚌 🏓 💔 🌶 ♥️ 🍭 🗓 🔆 💃 🍑 🍪 👛 🚨 💡 💜 🚲 💼 🍦 📂 🎳 🍒 ⏺ 💍 📟 📋 🚲 🍡 🙌 🍯 🕒 🕰 📲 🍯 🔊 💈 👃 🤑 📲 🚼 💜 🛄 ⛰ 👕 💊 🚢 🈺 💗 🚻 🎚 🔏 👲 🈶 ㊗️ 🐑 ▫️ 👮 📌 💳 🛬 🏆 🛳 ❔ 🕌 📐 👸 🐀 🎒 👑 Ⓜ️ 👪 🛐 😻 🐏 🍲 👹 🎎 ✴️ 🙃 🚲 🌦 🔊 🏁 ⛺️ 🔧 ⏺ 🏹 🐹 🆖 🐡 🏰 🌆 💢 ❔ ⚫️ 🎛 🏯 🔖 💙 ⏪ 😅 💰 🌌 💫 😕 🎫 🛣 🔋 ↔️ 🔽 😗 🗂 🏢 🏕"_s);
BENCHMARK_CAPTURE(
    BM_EmojiParsing, random_emoji_with_text,
    u"Lorem ipsum 🙏 dolor 📑 sit 🌷 amet, 🌚 consectetur 🕔 adipiscing ✅ elit, 📤 sed 🖨 do 🍬 eiusmod 🔫 tempor 🐋 incididunt 🏛 ut 🔱 labore 🔓 et ⤵️ dolore 📫 magna 🏓 aliqua. 🚋 Ut ⚛ enim 🏄 ad 🗼 minim 🏜 veniam, 🖇 quis 📌 nostrud 😈 exercitation ↙️ ullamco ®️ laboris 👁 nisi 💹 ut 🔜 aliquip ☠ ex ⛱ ea 🛢 commodo 🍝 consequat. 🐶 Duis 8️⃣ aute ➕ irure ♣️ dolor 🍀 in 🔆 reprehenderit 🍒 in 🎣 voluptate ⏮ velit #️⃣ esse 🔥 cillum ◼️ dolore 🖍 eu 😗 fugiat ⤴️ nulla 🦃 pariatur. ❕ Excepteur ➰ sint 🐄 occaecat 🚈 cupidatat 🕰 non 😞 proident, 📲 sunt 🛩 in ⚡️ culpa 7️⃣ qui 🎬 officia 🛍 deserunt 🔚"_s);
