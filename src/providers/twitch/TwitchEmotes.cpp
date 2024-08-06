#include "providers/twitch/TwitchEmotes.hpp"

#include "common/Literals.hpp"
#include "common/QLogging.hpp"
#include "common/UniqueAccess.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "util/QStringHash.hpp"

#include <QStringBuilder>

namespace {

using namespace chatterino;

Url getEmoteLink(const EmoteId &id, const QString &emoteScale)
{
    return {TWITCH_EMOTE_TEMPLATE.arg(id.string, emoteScale)};
}

QSize getEmoteExpectedBaseSize(const EmoteId &id)
{
    // From Twitch docs - expected size for an emote (1x)
    constexpr QSize defaultBaseSize(28, 28);
    static std::unordered_map<QString, QSize> outliers{
        {"555555635", {21, 18}}, /* ;p */
        {"555555636", {21, 18}}, /* ;-p */
        {"555555614", {21, 18}}, /* O_o */
        {"555555641", {21, 18}}, /* :z */
        {"555555604", {21, 18}}, /* :\\ */
        {"444", {21, 18}},       /* :| */
        {"555555634", {21, 18}}, /* ;-P */
        {"439", {21, 18}},       /* ;) */
        {"555555642", {21, 18}}, /* :-z */
        {"555555613", {21, 18}}, /* :-o */
        {"555555625", {21, 18}}, /* :-p */
        {"433", {21, 18}},       /* :/ */
        {"555555622", {21, 18}}, /* :P */
        {"555555640", {21, 18}}, /* :-| */
        {"555555623", {21, 18}}, /* :-P */
        {"555555628", {21, 18}}, /* :) */
        {"555555632", {21, 18}}, /* 8-) */
        {"555555667", {20, 18}}, /* ;p */
        {"445", {21, 18}},       /* <3 */
        {"555555668", {20, 18}}, /* ;-p */
        {"555555679", {20, 18}}, /* :z */
        {"483", {20, 18}},       /* <3 */
        {"555555666", {20, 18}}, /* ;-P */
        {"497", {20, 18}},       /* O_o */
        {"555555664", {20, 18}}, /* :-p */
        {"555555671", {20, 18}}, /* :o */
        {"555555681", {20, 18}}, /* :Z */
        {"555555672", {20, 18}}, /* :-o */
        {"555555676", {20, 18}}, /* :-\\ */
        {"555555611", {21, 18}}, /* :-O */
        {"555555670", {20, 18}}, /* :-O */
        {"555555688", {20, 18}}, /* :-D */
        {"441", {21, 18}},       /* B) */
        {"555555601", {21, 18}}, /* >( */
        {"491", {20, 18}},       /* ;P */
        {"496", {20, 18}},       /* :D */
        {"492", {20, 18}},       /* :O */
        {"555555573", {24, 18}}, /* o_O */
        {"555555643", {21, 18}}, /* :Z */
        {"1898", {26, 28}},      /* ThunBeast */
        {"555555682", {20, 18}}, /* :-Z */
        {"1896", {20, 30}},      /* WholeWheat */
        {"1906", {24, 30}},      /* SoBayed */
        {"555555607", {21, 18}}, /* :-( */
        {"555555660", {20, 18}}, /* :-( */
        {"489", {20, 18}},       /* :( */
        {"495", {20, 18}},       /* :s */
        {"555555638", {21, 18}}, /* :-D */
        {"357", {28, 30}},       /* HotPokket */
        {"555555624", {21, 18}}, /* :p */
        {"73", {21, 30}},        /* DBstyle */
        {"555555674", {20, 18}}, /* :-/ */
        {"555555629", {21, 18}}, /* :-) */
        {"555555600", {24, 18}}, /* R-) */
        {"41", {19, 27}},        /* Kreygasm */
        {"555555612", {21, 18}}, /* :o */
        {"488", {29, 24}},       /* :7 */
        {"69", {41, 28}},        /* BloodTrail */
        {"555555608", {21, 18}}, /* R) */
        {"501", {20, 18}},       /* ;) */
        {"50", {18, 27}},        /* ArsonNoSexy */
        {"443", {21, 18}},       /* :D */
        {"1904", {24, 30}},      /* BigBrother */
        {"555555595", {24, 18}}, /* ;P */
        {"555555663", {20, 18}}, /* :p */
        {"555555576", {24, 18}}, /* o.o */
        {"360", {22, 30}},       /* FailFish */
        {"500", {20, 18}},       /* B) */
        {"3", {24, 18}},         /* :D */
        {"484", {20, 22}},       /* R) */
        {"555555678", {20, 18}}, /* :-| */
        {"7", {24, 18}},         /* B) */
        {"52", {32, 32}},        /* SMOrc */
        {"555555644", {21, 18}}, /* :-Z */
        {"18", {20, 27}},        /* TheRinger */
        {"49106", {27, 28}},     /* CorgiDerp */
        {"6", {24, 18}},         /* O_o */
        {"10", {24, 18}},        /* :/ */
        {"47", {24, 24}},        /* PunchTrees */
        {"555555561", {24, 18}}, /* :-D */
        {"555555564", {24, 18}}, /* :-| */
        {"13", {24, 18}},        /* ;P */
        {"555555593", {24, 18}}, /* :p */
        {"555555589", {24, 18}}, /* ;) */
        {"555555590", {24, 18}}, /* ;-) */
        {"486", {27, 42}},       /* :> */
        {"40", {21, 27}},        /* KevinTurtle */
        {"555555558", {24, 18}}, /* :( */
        {"555555597", {24, 18}}, /* ;p */
        {"555555580", {24, 18}}, /* :O */
        {"555555567", {24, 18}}, /* :Z */
        {"1", {24, 18}},         /* :) */
        {"11", {24, 18}},        /* ;) */
        {"33", {25, 32}},        /* DansGame */
        {"555555586", {24, 18}}, /* :-/ */
        {"4", {24, 18}},         /* >( */
        {"555555588", {24, 18}}, /* :-\\ */
        {"12", {24, 18}},        /* :P */
        {"555555563", {24, 18}}, /* :| */
        {"555555581", {24, 18}}, /* :-O */
        {"555555598", {24, 18}}, /* ;-p */
        {"555555596", {24, 18}}, /* ;-P */
        {"555555557", {24, 18}}, /* :-) */
        {"498", {20, 18}},       /* >( */
        {"555555680", {20, 18}}, /* :-z */
        {"555555587", {24, 18}}, /* :\\ */
        {"5", {24, 18}},         /* :| */
        {"354", {20, 30}},       /* 4Head */
        {"555555562", {24, 18}}, /* >( */
        {"555555594", {24, 18}}, /* :-p */
        {"490", {20, 18}},       /* :P */
        {"555555662", {20, 18}}, /* :-P */
        {"2", {24, 18}},         /* :( */
        {"1902", {27, 29}},      /* Keepo */
        {"555555627", {21, 18}}, /* ;-) */
        {"555555566", {24, 18}}, /* :-z */
        {"555555559", {24, 18}}, /* :-( */
        {"555555592", {24, 18}}, /* :-P */
        {"28", {39, 27}},        /* MrDestructoid */
        {"8", {24, 18}},         /* :O */
        {"244", {24, 30}},       /* FUNgineer */
        {"555555591", {24, 18}}, /* :P */
        {"555555585", {24, 18}}, /* :/ */
        {"494", {20, 18}},       /* :| */
        {"9", {24, 18}},         /* <3 */
        {"555555584", {24, 18}}, /* <3 */
        {"555555579", {24, 18}}, /* 8-) */
        {"14", {24, 18}},        /* R) */
        {"485", {27, 18}},       /* #/ */
        {"555555560", {24, 18}}, /* :D */
        {"86", {36, 30}},        /* BibleThump */
        {"555555578", {24, 18}}, /* B-) */
        {"17", {20, 27}},        /* StoneLightning */
        {"436", {21, 18}},       /* :O */
        {"555555675", {20, 18}}, /* :\\ */
        {"22", {19, 27}},        /* RedCoat */
        {"555555574", {24, 18}}, /* o.O */
        {"555555603", {21, 18}}, /* :-/ */
        {"1901", {24, 28}},      /* Kippa */
        {"15", {21, 27}},        /* JKanStyle */
        {"555555605", {21, 18}}, /* :-\\ */
        {"555555701", {20, 18}}, /* ;-) */
        {"487", {20, 42}},       /* <] */
        {"555555572", {24, 18}}, /* O.O */
        {"65", {40, 30}},        /* FrankerZ */
        {"25", {25, 28}},        /* Kappa */
        {"36", {36, 30}},        /* PJSalt */
        {"499", {20, 18}},       /* :) */
        {"555555565", {24, 18}}, /* :z */
        {"434", {21, 18}},       /* :( */
        {"555555577", {24, 18}}, /* B) */
        {"34", {21, 28}},        /* SwiftRage */
        {"555555575", {24, 18}}, /* o_o */
        {"92", {23, 30}},        /* PMSTwin */
        {"555555570", {24, 18}}, /* O.o */
        {"555555569", {24, 18}}, /* O_o */
        {"493", {20, 18}},       /* :/ */
        {"26", {20, 27}},        /* JonCarnage */
        {"66", {20, 27}},        /* OneHand */
        {"555555568", {24, 18}}, /* :-Z */
        {"555555599", {24, 18}}, /* R) */
        {"1900", {33, 30}},      /* RalpherZ */
        {"555555582", {24, 18}}, /* :o */
        {"1899", {22, 30}},      /* TF2John */
        {"555555633", {21, 18}}, /* ;P */
        {"16", {22, 27}},        /* OptimizePrime */
        {"30", {29, 27}},        /* BCWarrior */
        {"555555583", {24, 18}}, /* :-o */
        {"32", {21, 27}},        /* GingerPower */
        {"87", {24, 30}},        /* ShazBotstix */
        {"74", {24, 30}},        /* AsianGlow */
        {"555555571", {24, 18}}, /* O_O */
        {"46", {24, 24}},        /* SSSsss */
    };

    auto it = outliers.find(id.string);
    if (it != outliers.end())
    {
        return it->second;
    }

    return defaultBaseSize;
}

qreal getEmote3xScaleFactor(const EmoteId &id)
{
    // From Twitch docs - expected size for an emote (1x)
    constexpr qreal default3xScaleFactor = 0.25;
    static std::unordered_map<QString, qreal> outliers{
        {"555555635", 0.3333333333333333},  /* ;p */
        {"555555636", 0.3333333333333333},  /* ;-p */
        {"555555614", 0.3333333333333333},  /* O_o */
        {"555555641", 0.3333333333333333},  /* :z */
        {"555555604", 0.3333333333333333},  /* :\\ */
        {"444", 0.3333333333333333},        /* :| */
        {"555555634", 0.3333333333333333},  /* ;-P */
        {"439", 0.3333333333333333},        /* ;) */
        {"555555642", 0.3333333333333333},  /* :-z */
        {"555555613", 0.3333333333333333},  /* :-o */
        {"555555625", 0.3333333333333333},  /* :-p */
        {"433", 0.3333333333333333},        /* :/ */
        {"555555622", 0.3333333333333333},  /* :P */
        {"555555640", 0.3333333333333333},  /* :-| */
        {"555555623", 0.3333333333333333},  /* :-P */
        {"555555628", 0.3333333333333333},  /* :) */
        {"555555632", 0.3333333333333333},  /* 8-) */
        {"555555667", 0.3333333333333333},  /* ;p */
        {"445", 0.3333333333333333},        /* <3 */
        {"555555668", 0.3333333333333333},  /* ;-p */
        {"555555679", 0.3333333333333333},  /* :z */
        {"483", 0.3333333333333333},        /* <3 */
        {"555555666", 0.3333333333333333},  /* ;-P */
        {"497", 0.3333333333333333},        /* O_o */
        {"555555664", 0.3333333333333333},  /* :-p */
        {"555555671", 0.3333333333333333},  /* :o */
        {"555555681", 0.3333333333333333},  /* :Z */
        {"555555672", 0.3333333333333333},  /* :-o */
        {"555555676", 0.3333333333333333},  /* :-\\ */
        {"555555611", 0.3333333333333333},  /* :-O */
        {"555555670", 0.3333333333333333},  /* :-O */
        {"555555688", 0.3333333333333333},  /* :-D */
        {"441", 0.3333333333333333},        /* B) */
        {"555555601", 0.3333333333333333},  /* >( */
        {"491", 0.3333333333333333},        /* ;P */
        {"496", 0.3333333333333333},        /* :D */
        {"492", 0.3333333333333333},        /* :O */
        {"555555573", 0.3333333333333333},  /* o_O */
        {"555555643", 0.3333333333333333},  /* :Z */
        {"1898", 0.3333333333333333},       /* ThunBeast */
        {"555555682", 0.3333333333333333},  /* :-Z */
        {"1896", 0.3333333333333333},       /* WholeWheat */
        {"1906", 0.3333333333333333},       /* SoBayed */
        {"555555607", 0.3333333333333333},  /* :-( */
        {"555555660", 0.3333333333333333},  /* :-( */
        {"489", 0.3333333333333333},        /* :( */
        {"495", 0.3333333333333333},        /* :s */
        {"555555638", 0.3333333333333333},  /* :-D */
        {"357", 0.3333333333333333},        /* HotPokket */
        {"555555624", 0.3333333333333333},  /* :p */
        {"73", 0.3333333333333333},         /* DBstyle */
        {"555555674", 0.3333333333333333},  /* :-/ */
        {"555555629", 0.3333333333333333},  /* :-) */
        {"555555600", 0.3333333333333333},  /* R-) */
        {"41", 0.3333333333333333},         /* Kreygasm */
        {"555555612", 0.3333333333333333},  /* :o */
        {"488", 0.3333333333333333},        /* :7 */
        {"69", 0.3333333333333333},         /* BloodTrail */
        {"555555608", 0.3333333333333333},  /* R) */
        {"501", 0.3333333333333333},        /* ;) */
        {"50", 0.3333333333333333},         /* ArsonNoSexy */
        {"443", 0.3333333333333333},        /* :D */
        {"1904", 0.3333333333333333},       /* BigBrother */
        {"555555595", 0.3333333333333333},  /* ;P */
        {"555555663", 0.3333333333333333},  /* :p */
        {"555555576", 0.3333333333333333},  /* o.o */
        {"360", 0.3333333333333333},        /* FailFish */
        {"500", 0.3333333333333333},        /* B) */
        {"3", 0.3333333333333333},          /* :D */
        {"484", 0.3333333333333333},        /* R) */
        {"555555678", 0.3333333333333333},  /* :-| */
        {"7", 0.3333333333333333},          /* B) */
        {"52", 0.3333333333333333},         /* SMOrc */
        {"555555644", 0.3333333333333333},  /* :-Z */
        {"18", 0.3333333333333333},         /* TheRinger */
        {"49106", 0.3333333333333333},      /* CorgiDerp */
        {"6", 0.3333333333333333},          /* O_o */
        {"10", 0.3333333333333333},         /* :/ */
        {"47", 0.3333333333333333},         /* PunchTrees */
        {"555555561", 0.3333333333333333},  /* :-D */
        {"555555564", 0.3333333333333333},  /* :-| */
        {"13", 0.3333333333333333},         /* ;P */
        {"555555593", 0.3333333333333333},  /* :p */
        {"555555589", 0.3333333333333333},  /* ;) */
        {"555555590", 0.3333333333333333},  /* ;-) */
        {"486", 0.3333333333333333},        /* :> */
        {"40", 0.3333333333333333},         /* KevinTurtle */
        {"555555558", 0.3333333333333333},  /* :( */
        {"555555597", 0.3333333333333333},  /* ;p */
        {"555555580", 0.3333333333333333},  /* :O */
        {"555555567", 0.3333333333333333},  /* :Z */
        {"1", 0.3333333333333333},          /* :) */
        {"11", 0.3333333333333333},         /* ;) */
        {"33", 0.3333333333333333},         /* DansGame */
        {"555555586", 0.3333333333333333},  /* :-/ */
        {"4", 0.3333333333333333},          /* >( */
        {"555555588", 0.3333333333333333},  /* :-\\ */
        {"12", 0.3333333333333333},         /* :P */
        {"555555563", 0.3333333333333333},  /* :| */
        {"555555581", 0.3333333333333333},  /* :-O */
        {"555555598", 0.3333333333333333},  /* ;-p */
        {"555555596", 0.3333333333333333},  /* ;-P */
        {"555555557", 0.3333333333333333},  /* :-) */
        {"498", 0.3333333333333333},        /* >( */
        {"555555680", 0.3333333333333333},  /* :-z */
        {"555555587", 0.3333333333333333},  /* :\\ */
        {"5", 0.3333333333333333},          /* :| */
        {"354", 0.3333333333333333},        /* 4Head */
        {"555555562", 0.3333333333333333},  /* >( */
        {"555555594", 0.3333333333333333},  /* :-p */
        {"490", 0.3333333333333333},        /* :P */
        {"555555662", 0.3333333333333333},  /* :-P */
        {"2", 0.3333333333333333},          /* :( */
        {"1902", 0.3333333333333333},       /* Keepo */
        {"555555627", 0.3333333333333333},  /* ;-) */
        {"555555566", 0.3333333333333333},  /* :-z */
        {"555555559", 0.3333333333333333},  /* :-( */
        {"555555592", 0.3333333333333333},  /* :-P */
        {"28", 0.3333333333333333},         /* MrDestructoid */
        {"8", 0.3333333333333333},          /* :O */
        {"244", 0.3333333333333333},        /* FUNgineer */
        {"555555591", 0.3333333333333333},  /* :P */
        {"555555585", 0.3333333333333333},  /* :/ */
        {"494", 0.3333333333333333},        /* :| */
        {"9", 0.21428571428571427},         /* <3 */
        {"555555584", 0.21428571428571427}, /* <3 */
        {"555555579", 0.3333333333333333},  /* 8-) */
        {"14", 0.3333333333333333},         /* R) */
        {"485", 0.3333333333333333},        /* #/ */
        {"555555560", 0.3333333333333333},  /* :D */
        {"86", 0.3333333333333333},         /* BibleThump */
        {"555555578", 0.3333333333333333},  /* B-) */
        {"17", 0.3333333333333333},         /* StoneLightning */
        {"436", 0.3333333333333333},        /* :O */
        {"555555675", 0.3333333333333333},  /* :\\ */
        {"22", 0.3333333333333333},         /* RedCoat */
        {"245", 0.3333333333333333},        /* ResidentSleeper */
        {"555555574", 0.3333333333333333},  /* o.O */
        {"555555603", 0.3333333333333333},  /* :-/ */
        {"1901", 0.3333333333333333},       /* Kippa */
        {"15", 0.3333333333333333},         /* JKanStyle */
        {"555555605", 0.3333333333333333},  /* :-\\ */
        {"555555701", 0.3333333333333333},  /* ;-) */
        {"487", 0.3333333333333333},        /* <] */
        {"22639", 0.3333333333333333},      /* BabyRage */
        {"555555572", 0.3333333333333333},  /* O.O */
        {"65", 0.3333333333333333},         /* FrankerZ */
        {"25", 0.3333333333333333},         /* Kappa */
        {"36", 0.3333333333333333},         /* PJSalt */
        {"499", 0.3333333333333333},        /* :) */
        {"555555565", 0.3333333333333333},  /* :z */
        {"434", 0.3333333333333333},        /* :( */
        {"555555577", 0.3333333333333333},  /* B) */
        {"34", 0.3333333333333333},         /* SwiftRage */
        {"555555575", 0.3333333333333333},  /* o_o */
        {"92", 0.3333333333333333},         /* PMSTwin */
        {"555555570", 0.3333333333333333},  /* O.o */
        {"555555569", 0.3333333333333333},  /* O_o */
        {"493", 0.3333333333333333},        /* :/ */
        {"26", 0.3333333333333333},         /* JonCarnage */
        {"66", 0.3333333333333333},         /* OneHand */
        {"973", 0.3333333333333333},        /* DAESuppy */
        {"555555568", 0.3333333333333333},  /* :-Z */
        {"555555599", 0.3333333333333333},  /* R) */
        {"1900", 0.3333333333333333},       /* RalpherZ */
        {"555555582", 0.3333333333333333},  /* :o */
        {"1899", 0.3333333333333333},       /* TF2John */
        {"555555633", 0.3333333333333333},  /* ;P */
        {"16", 0.3333333333333333},         /* OptimizePrime */
        {"30", 0.3333333333333333},         /* BCWarrior */
        {"555555583", 0.3333333333333333},  /* :-o */
        {"32", 0.3333333333333333},         /* GingerPower */
        {"87", 0.3333333333333333},         /* ShazBotstix */
        {"74", 0.3333333333333333},         /* AsianGlow */
        {"555555571", 0.3333333333333333},  /* O_O */
        {"46", 0.3333333333333333},         /* SSSsss */
    };

    auto it = outliers.find(id.string);
    if (it != outliers.end())
    {
        return it->second;
    }

    return default3xScaleFactor;
}

}  // namespace

namespace chatterino {

using namespace literals;

QString TwitchEmoteSet::title() const
{
    if (!this->owner || this->owner->name.isEmpty())
    {
        return "Twitch";
    }
    if (this->isBits)
    {
        return this->owner->name + " (Bits)";
    }

    return this->owner->name;
}

QString TwitchEmotes::cleanUpEmoteCode(const QString &dirtyEmoteCode)
{
    auto cleanCode = dirtyEmoteCode;
    cleanCode.detach();

    static QMap<QString, QString> emoteNameReplacements{
        {"[oO](_|\\.)[oO]", "O_o"}, {"\\&gt\\;\\(", "&gt;("},
        {"\\&lt\\;3", "&lt;3"},     {"\\:-?(o|O)", ":O"},
        {"\\:-?(p|P)", ":P"},       {"\\:-?[\\\\/]", ":/"},
        {"\\:-?[z|Z|\\|]", ":Z"},   {"\\:-?\\(", ":("},
        {"\\:-?\\)", ":)"},         {"\\:-?D", ":D"},
        {"\\;-?(p|P)", ";P"},       {"\\;-?\\)", ";)"},
        {"R-?\\)", "R)"},           {"B-?\\)", "B)"},
    };

    auto it = emoteNameReplacements.find(dirtyEmoteCode);
    if (it != emoteNameReplacements.end())
    {
        cleanCode = it.value();
    }

    return cleanCode;
}

// id is used for lookup
// emoteName is used for giving a name to the emote in case it doesn't exist
EmotePtr TwitchEmotes::getOrCreateEmote(const EmoteId &id,
                                        const EmoteName &name_)
{
    auto name = TwitchEmotes::cleanUpEmoteCode(name_.string);

    // search in cache or create new emote
    auto cache = this->twitchEmotesCache_.access();
    auto shared = (*cache)[id].lock();

    if (!shared)
    {
        auto baseSize = getEmoteExpectedBaseSize(id);
        auto emote3xScaleFactor = getEmote3xScaleFactor(id);
        (*cache)[id] = shared = std::make_shared<Emote>(Emote{
            EmoteName{name},
            ImageSet{
                Image::fromUrl(getEmoteLink(id, "1.0"), 1, baseSize),
                Image::fromUrl(getEmoteLink(id, "2.0"), 0.5, baseSize * 2),
                Image::fromUrl(getEmoteLink(id, "3.0"), emote3xScaleFactor,
                               baseSize * (1.0 / emote3xScaleFactor)),
            },
            Tooltip{name.toHtmlEscaped() + "<br>Twitch Emote"},
        });
    }

    return shared;
}

TwitchEmoteSetMeta getTwitchEmoteSetMeta(const HelixChannelEmote &emote)
{
    bool isSub = emote.type == u"subscriptions";
    bool isBits = emote.type == u"bitstier";
    bool isSubLike = isSub || isBits;

    // A lot of emotes don't have their emote-set-id set, so we create a
    // virtual emote set that groups emotes by the owner.
    // Additionally, a lot of emote sets are small, so they're grouped together as globals.
    auto actualSetID = [&]() -> QString {
        if (!isSub && !isBits)
        {
            return u"x-c2-globals"_s;
        }

        if (!emote.setID.isEmpty())
        {
            return emote.setID;
        }

        if (isSub)
        {
            return TWITCH_SUB_EMOTE_SET_PREFIX % emote.ownerID;
        }
        // isBits
        return TWITCH_BIT_EMOTE_SET_PREFIX % emote.ownerID;
    }();

    return {
        .setID = actualSetID,
        .isBits = isBits,
        .isSubLike = isSubLike,
    };
}

}  // namespace chatterino
