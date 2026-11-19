// Host-side tests for the pure logic (no console needed). Run with tests/run.sh.
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "app.h"
#include "badge.h"
#include "coversync.h"
#include "covers.h"
#include "default_rules.h"
#include "frame.h"
#include "fsx.h"
#include "gfx.h"
#include "isoinfo.h"
#include "library.h"
#include "mygames.h"
#include "platform.h"
#include "sha1.h"
#include "sorter.h"
#include "tasks.h"

static int g_failed = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_failed;                                                 \
        }                                                               \
    } while (0)

#define CHECK_EQ(a, b)                                                                                  \
    do {                                                                                                \
        const auto va_ = (a);                                                                           \
        const auto vb_ = (b);                                                                           \
        if (!(va_ == vb_)) {                                                                            \
            std::printf("FAIL %s:%d  %s == %s\n", __FILE__, __LINE__, #a, #b);                           \
            ++g_failed;                                                                                 \
        }                                                                                               \
    } while (0)

using Bytes = std::vector<std::uint8_t>;

// ---------------------------------------------------------------- sorter --

static std::vector<sorter::Rule> rules() { return sorter::parseRules(kDefaultRules); }

static std::vector<std::string> sortTitles(std::vector<std::string> titles) {
    const auto r = rules();
    std::stable_sort(titles.begin(), titles.end(), [&](const std::string& a, const std::string& b) {
        return sorter::less(sorter::makeKey(a, r), sorter::makeKey(b, r));
    });
    return titles;
}

static void testNormalize() {
    CHECK_EQ(sorter::normalize("Ratchet & Clank"), std::string("ratchet clank"));
    CHECK_EQ(sorter::normalize("The Sly Collection"), std::string("sly collection"));
    CHECK_EQ(sorter::normalize("Luigi's Mansion"), std::string("luigis mansion"));
    CHECK_EQ(sorter::normalize("Pok\xC3\xA9mon"), std::string("pokemon"));
    CHECK_EQ(sorter::normalize("PLAYSTATION\xC2\xAE" "3"), std::string("playstation 3"));
}

static void testCleanTitle() {
    CHECK_EQ(sorter::cleanTitle("Alpha Protocol (USA) (En,Fr,De,Es,It)"), std::string("Alpha Protocol"));
    CHECK_EQ(sorter::cleanTitle("Infamous (USA) (En,Fr,Es) (v02.00)"), std::string("Infamous"));
    CHECK_EQ(sorter::cleanTitle("Game [!] (Europe)"), std::string("Game"));
    CHECK_EQ(sorter::cleanTitle("Final Fantasy VII (USA) (Disc 2)"), std::string("Final Fantasy VII (Disc 2)"));
    CHECK_EQ(sorter::cleanTitle("(USA)"), std::string("(USA)"));  // never empty
    CHECK_EQ(sorter::cleanTitle("Broken (tag"), std::string("Broken (tag"));
    CHECK_EQ(sorter::prettyTitle("God of War - Ascension (USA)"), std::string("God of War: Ascension"));
}

static void testRules() {
    const auto r = sorter::parseRules("# c\nfoo bar | Series X | 2010-04\nbaz | y | 1999  # c\nbad line\n");
    CHECK_EQ(r.size(), size_t(2));
    CHECK_EQ(r[0].date, 201004);
    CHECK_EQ(r[1].date, 199900);
    CHECK_EQ(r[1].series, std::string("y"));
    CHECK(rules().size() > 200);
}

static void testSeriesOrder() {
    // Series sort under their own name; inside a series the release date decides.
    using V = std::vector<std::string>;
    CHECK_EQ(sortTitles({"LittleBigPlanet 3 (USA)", "LittleBigPlanet (USA)", "LittleBigPlanet Karting (USA)",
                         "LittleBigPlanet 2 (USA)"}),
             (V{"LittleBigPlanet (USA)", "LittleBigPlanet 2 (USA)", "LittleBigPlanet Karting (USA)",
                "LittleBigPlanet 3 (USA)"}));
    CHECK_EQ(sortTitles({"Ratchet & Clank - Into the Nexus (USA)", "Ratchet & Clank Future - A Crack in Time (USA)",
                         "Ratchet & Clank Future - Tools of Destruction (USA)"}),
             (V{"Ratchet & Clank Future - Tools of Destruction (USA)", "Ratchet & Clank Future - A Crack in Time (USA)",
                "Ratchet & Clank - Into the Nexus (USA)"}));
    CHECK_EQ(sortTitles({"Silent Hill - Downpour (USA)", "Silent Hill - Homecoming (USA)"}),
             (V{"Silent Hill - Homecoming (USA)", "Silent Hill - Downpour (USA)"}));
    CHECK_EQ(sortTitles({"Tales of Xillia 2 (USA)", "Tales of Xillia (USA)", "Tales of Graces f (USA)",
                         "Tales of Symphonia Chronicles (USA)"}),
             (V{"Tales of Graces f (USA)", "Tales of Xillia (USA)", "Tales of Symphonia Chronicles (USA)",
                "Tales of Xillia 2 (USA)"}));
    // whole words only: "god of war ii" must not catch "god of war iii"
    CHECK_EQ(sortTitles({"God of War III", "God of War II", "God of War"}), (V{"God of War", "God of War II", "God of War III"}));
    // unlisted sequels stay together and ordered by number, a subtitle without number goes first
    CHECK_EQ(sortTitles({"Zork 3", "Zork: Grand Inquisitor", "Zork 2", "Alpha"}),
             (V{"Alpha", "Zork: Grand Inquisitor", "Zork 2", "Zork 3"}));
    // multi disc
    CHECK_EQ(sortTitles({"Fake Game (Disc 2)", "Fake Game (Disc 1)"}), (V{"Fake Game (Disc 1)", "Fake Game (Disc 2)"}));
    // the user rules win over the built-in table
    auto user = sorter::parseRules("god of war | zzz | 2000\n");
    for (const auto& x : rules()) user.push_back(x);
    CHECK_EQ(sorter::makeKey("God of War", user).series, std::string("zzz"));
}

// Final Fantasy Tactics is not a numbered Final Fantasy: it is a title of its own, so the alphabetical order of the
// series names places it, not the release dates of Final Fantasy VII to XIV.
static void testFinalFantasyTactics() {
    using V = std::vector<std::string>;
    const auto r = rules();
    CHECK_EQ(sorter::makeKey("Final Fantasy Tactics (USA)", r).series, std::string("final fantasy tactics"));
    CHECK_EQ(sorter::makeKey("Final Fantasy VII (USA) (Disc 1)", r).series, std::string("final fantasy"));
    CHECK_EQ(sortTitles({"Final Fantasy Tactics (USA)", "Final Fantasy XIV (USA)", "Final Fantasy VIII (USA) (Disc 1)",
                         "Final Fantasy IX (USA) (Disc 1)", "Final Fantasy VII (USA) (Disc 1)", "Final Fantasy X (USA)", "Folklore (USA)"}),
             (V{"Final Fantasy VII (USA) (Disc 1)", "Final Fantasy VIII (USA) (Disc 1)", "Final Fantasy IX (USA) (Disc 1)",
                "Final Fantasy X (USA)", "Final Fantasy XIV (USA)", "Final Fantasy Tactics (USA)", "Folklore (USA)"}));
}

// The classic Tomb Raiders are ordered by release date: the title that carries no number ("The Last Revelation")
// must not jump ahead of II and III the way an unlisted subtitle without a number does.
static void testTombRaiderOrder() {
    using V = std::vector<std::string>;
    const V byDate = {"Tomb Raider (USA)", "Tomb Raider II - Starring Lara Croft (USA)",
                      "Tomb Raider III - Adventures of Lara Croft (USA)", "Tomb Raider - The Last Revelation (USA)",
                      "Tomb Raider Chronicles (USA)", "Tomb Raider - The Angel of Darkness (USA)", "Tomb Raider Legend",
                      "Tomb Raider Anniversary", "Tomb Raider Underworld", "Tomb Raider Trilogy"};
    V shuffled = byDate;
    std::reverse(shuffled.begin(), shuffled.end());
    CHECK_EQ(sortTitles(shuffled), byDate);
    // the names the console shows (the hyphen turned into a colon, the "The" kept)
    CHECK_EQ(sortTitles({"Tomb Raider: The Last Revelation", "Tomb Raider III: Adventures of Lara Croft", "Tomb Raider II: Starring Lara Croft",
                         "Tomb Raider"}),
             (V{"Tomb Raider", "Tomb Raider II: Starring Lara Croft", "Tomb Raider III: Adventures of Lara Croft",
                "Tomb Raider: The Last Revelation"}));
    // they stay together, between Tom Clancy and Tony Hawk
    CHECK_EQ(sortTitles({"Tony Hawk's Pro Skater", "Tomb Raider II", "Tekken 3", "Tomb Raider"}),
             (V{"Tekken 3", "Tomb Raider", "Tomb Raider II", "Tony Hawk's Pro Skater"}));
}

// Same defect as Tomb Raider, found in the user's PS1 list: a title without a number in its name ("Year of the Dragon",
// "Mythologies") used to jump ahead of the numbered ones, so Spyro 3 came before Spyro 2.
static void testPs1SequelsWithoutNumbers() {
    using V = std::vector<std::string>;
    CHECK_EQ(sortTitles({"Spyro: Year of the Dragon", "Spyro 2: Ripto's Rage!", "Spyro the Dragon"}),
             (V{"Spyro the Dragon", "Spyro 2: Ripto's Rage!", "Spyro: Year of the Dragon"}));
    CHECK_EQ(sortTitles({"Mortal Kombat Mythologies: Sub-Zero", "Mortal Kombat Trilogy"}),
             (V{"Mortal Kombat Trilogy", "Mortal Kombat Mythologies: Sub-Zero"}));
}

// The list shown by the user's console: the new order must be the expected one.
static void testUserLibrary() {
    const std::vector<std::string> shelf = {
        "Alpha Protocol (USA) (En,Fr,De,Es,It)", "Drakengard 3 (USA) (En,Fr)", "Enchanted Arms (USA)",
        "Eternal Sonata (USA) (En,Fr)", "Folklore (USA) (En,Fr,De,Es,It)", "God of War - Ascension (USA) (En,Fr,Es,Pt)",
        "Guitar Hero III: Legends of Rock", "Heavenly Sword (USA) (En,Fr,De,Es,It)", "Infamous (USA) (En,Fr,Es) (v02.00)",
        "Infamous 2 (USA) (En,Fr,Es,Pt) (v02.00)", "Killzone 2 (USA) (En,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi,Pl,Ru)",
        "Killzone 3 (USA, Brazil) (En,Ja,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi,Zh,Ko,Pl,Ru,Tr)",
        "LittleBigPlanet (USA) (En,Ja,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi,Zh,Ko) (v02.00)",
        "LittleBigPlanet 2 (USA) (En,Fr,Es,Pt)", "LittleBigPlanet 3 (USA) (En,Fr)",
        "LittleBigPlanet Karting (USA, Brazil) (En,Fr,Es,Pt)", "PlayStation All-Stars Battle Royale (USA) (En,Fr,Es,Pt)",
        "Puppeteer (USA) (En,Fr,Es,Pt)", "Ratchet & Clank - Into the Nexus (USA) (En,Fr,Es)",
        "Ratchet & Clank Future - A Crack in Time (USA) (En,Fr,Es)",
        "Ratchet & Clank Future - Tools of Destruction (USA) (En,Ja,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi,Zh,Ko) (v02.00)",
        "Resistance - Fall of Man (USA) (En,Ja,Fr,De,Es,It,Nl,Pt,Ko)", "Resistance 2 (USA) (En,Ja,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi,Ko)",
        "Resistance 3 (USA) (En,Fr,Es)", "Silent Hill - Downpour (USA, Asia) (En,Fr,De,Es,It,Pt)",
        "Silent Hill - Homecoming (USA) (En,Fr,De,Es,It)", "Sorcery (USA) (En,Fr,Es)", "Tales of Graces f (USA)",
        "Tales of Xillia (USA) (En,Fr,Es)", "Tales of Xillia 2 (USA) (En,Fr,Es)",
        "White Knight Chronicles II (USA) (En,Fr,Es)", "Yakuza - Dead Souls (USA)",
        "Sly Cooper - Thieves in Time (USA) (En,Fr,Es,Pt)"};
    std::vector<std::string> shuffled = shelf;
    std::reverse(shuffled.begin(), shuffled.end());
    const auto sorted = sortTitles(shuffled);
    const std::vector<std::string> want = {
        "Alpha Protocol", "Drakengard 3", "Enchanted Arms", "Eternal Sonata", "Folklore", "God of War", "Guitar Hero",
        "Heavenly Sword", "Infamous", "Infamous 2", "Killzone 2", "Killzone 3", "LittleBigPlanet", "LittleBigPlanet 2",
        "LittleBigPlanet Karting", "LittleBigPlanet 3", "PlayStation All-Stars", "Puppeteer", "Tools of Destruction",
        "A Crack in Time", "Into the Nexus", "Fall of Man", "Resistance 2", "Resistance 3", "Homecoming", "Downpour",
        "Sly Cooper", "Sorcery", "Graces f", "Xillia (USA)", "Xillia 2", "White Knight Chronicles II", "Yakuza"};
    CHECK_EQ(sorted.size(), want.size());
    for (std::size_t i = 0; i < sorted.size() && i < want.size(); ++i) {
        if (sorted[i].find(want[i]) == std::string::npos) {
            std::printf("FAIL user library position %zu: got '%s', wanted '%s'\n", i, sorted[i].c_str(), want[i].c_str());
            ++g_failed;
        }
    }
}

// ------------------------------------------------------------- mygames --

static std::string entry(const std::string& key, const std::string& title, const std::string& file) {
    return "<T key=\"" + key + "\" include=\"inc\"><P key=\"icon\"><>/dev_hdd0/PS3ISO/" + file +
           ".png</></P><P key=\"title\"><>" + title + "</></P><P key=\"module_action\"><>/mount_ps3/dev_hdd0/PS3ISO/" +
           file + ".iso</></P><P key=\"info\"><>hdd0/PS3ISO</></P></T>";
}

static std::string tag(const std::string& key) {
    return "<I class=\"type:x-xmb/module-action\" key=\"" + key + "\" attr=\"" + key + "\"/>";
}

static std::string sampleXml(const std::vector<std::pair<std::string, std::string>>& games,
                             const std::vector<std::string>& order) {
    std::string x = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><X version=\"1.0\"><V id=\"seg_mygames\"><A><T key=\"wm_ps3\">"
                    "<P key=\"title\"><>PLAYSTATION\xC2\xAE" "3</></P><P key=\"info\"><>3 Jogos PS3</></P></T></A><I>"
                    "<Q class=\"type:x-xmb/folder-pixmap\" key=\"wm_ps3\" attr=\"wm_ps3\" src=\"#seg_wm_ps3_items\"/></I></V>"
                    "<V id=\"seg_wm_ps3_items\"><A><T key=\"inc\"><P key=\"module_name\"><>idle_plugin</></P></T>";
    for (const auto& g : games) x += entry(g.first, g.second, g.second);
    x += "</A><I>";
    for (const auto& k : order) x += tag(k);
    x += "</I></V></X>";
    return x;
}

static void testBalanced() {
    CHECK(mygames::balanced("<A><P key=\"t\"><>x</></P></A>"));
    CHECK(!mygames::balanced("<A><P></A>"));
    CHECK(!mygames::balanced("<A>"));
    CHECK(!mygames::balanced("<A></B>"));
    CHECK(mygames::balanced("<?xml version=\"1.0\"?><X><I a=\"1\"/></X>"));
}

static void testMygames() {
    const auto xml = sampleXml({{"0001", "Zeta"}, {"0002", "Alpha &amp; Beta"}, {"0003", "Mid"}}, {"0001", "0002", "0003"});
    const auto list = mygames::List::parse(xml);
    CHECK(list.has_value());
    if (!list) return;
    CHECK_EQ(list->segments().size(), size_t(1));
    const auto& seg = list->segments()[0];
    CHECK_EQ(seg.id, std::string("seg_wm_ps3_items"));
    CHECK_EQ(seg.platform, std::string("ps3"));
    CHECK_EQ(seg.games.size(), size_t(3));
    CHECK_EQ(seg.games[1].title, std::string("Alpha & Beta"));  // entities decoded
    CHECK_EQ(seg.games[1].icon, std::string("/dev_hdd0/PS3ISO/Alpha & Beta.png"));
    CHECK_EQ(seg.games[0].action, std::string("/mount_ps3/dev_hdd0/PS3ISO/Zeta.iso"));

    const auto out = list->reordered(0, {"0002", "0003", "0001"});
    CHECK(out.has_value());
    if (out) {
        CHECK_EQ(out->size(), xml.size());  // a permutation keeps the length
        CHECK(*out == sampleXml({{"0001", "Zeta"}, {"0002", "Alpha &amp; Beta"}, {"0003", "Mid"}}, {"0002", "0003", "0001"}));
        const auto again = mygames::List::parse(*out);
        CHECK(again.has_value());
        if (again) CHECK_EQ(again->segments()[0].games[0].key, std::string("0002"));
    }
    // not a permutation
    CHECK(!list->reordered(0, {"0002", "0003"}).has_value());
    CHECK(!list->reordered(0, {"0002", "0002", "0001"}).has_value());
    CHECK(!list->reordered(0, {"0002", "0003", "9999"}).has_value());
    CHECK(!list->reordered(5, {"0002"}).has_value());
    // unchanged order gives back the same text
    const auto same = list->reordered(0, {"0001", "0002", "0003"});
    CHECK(same.has_value() && *same == xml);

    // broken input
    CHECK(!mygames::List::parse("<X></X>").has_value());
    CHECK(!mygames::List::parse(sampleXml({{"0001", "A"}}, {"0001", "0777"})).has_value());   // order tag without definition
    CHECK(!mygames::List::parse(sampleXml({{"0001", "A"}}, {"0001", "0001"})).has_value());   // repeated key
    CHECK(!mygames::List::parse(xml.substr(0, xml.size() - 6)).has_value());                  // unbalanced
    CHECK_EQ(mygames::platformLabel("psx"), std::string("PS1"));
}

static void testRealXml() {
    const char* path = std::getenv("HOMETIDY_REAL_XML");
    if (!path) return;
    std::string text;
    CHECK(fsx::readFile(path, text));
    const auto list = mygames::List::parse(text);
    CHECK(list.has_value());
    if (!list) return;
    const auto r = rules();
    for (std::size_t s = 0; s < list->segments().size(); ++s) {
        std::vector<mygames::Game> games = list->segments()[s].games;
        std::stable_sort(games.begin(), games.end(), [&](const auto& a, const auto& b) {
            return sorter::less(sorter::makeKey(a.title, r), sorter::makeKey(b.title, r));
        });
        std::vector<std::string> order;
        for (const auto& g : games) order.push_back(g.key);
        const auto out = list->reordered(s, order);
        CHECK(out.has_value());
        if (out) CHECK_EQ(out->size(), text.size());
        std::printf("real xml %s:\n", list->segments()[s].id.c_str());
        for (const auto& g : games) std::printf("  %s\n", sorter::prettyTitle(g.title).c_str());
    }
}


// ----------------------------------------------------------------- badge --

// A 160x160 test cover: smooth colours, fully opaque.
static badge::Image makeCover(int w = 160, int h = 160, int alpha = 255) {
    badge::Image im;
    im.w = w;
    im.h = h;
    im.rgba.resize(std::size_t(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            std::uint8_t* p = &im.rgba[(std::size_t(y) * w + x) * 4];
            p[0] = std::uint8_t(40 + x * 120 / w);
            p[1] = std::uint8_t(60 + y * 100 / h);
            p[2] = std::uint8_t(200 - x * 80 / w);
            p[3] = std::uint8_t(alpha);
        }
    }
    return im;
}

static int maxDiff(const badge::Image& a, const badge::Image& b) {
    int worst = 0;
    for (std::size_t i = 0; i < a.rgba.size(); ++i) worst = std::max(worst, std::abs(int(a.rgba[i]) - int(b.rgba[i])));
    return worst;
}

static void testBadge() {
    const auto box = badge::boxFor(160, 160);
    CHECK_EQ(box.side, 24);
    CHECK_EQ(box.x, 6);
    CHECK_EQ(badge::boxFor(1024, 1024).side, 153);

    badge::Image plain = makeCover();
    CHECK(!badge::isMarked(plain));
    badge::Image marked = plain;
    badge::apply(marked);
    CHECK(badge::isMarked(marked));
    CHECK(maxDiff(plain, marked) > 100);
    // pixels far from the badge are untouched
    CHECK(std::equal(plain.rgba.begin() + (100 * 160 + 100) * 4, plain.rgba.begin() + (100 * 160 + 100) * 4 + 4,
                     marked.rgba.begin() + (100 * 160 + 100) * 4));
    // fill colour and the white check
    const std::uint8_t* fill = &marked.rgba[((box.y + 3) * 160 + box.x + box.side / 2) * 4];
    CHECK(fill[0] == 27 && fill[1] == 158 && fill[2] == 50 && fill[3] == 230);

    // transparent covers (the framed icons) are marked too
    badge::Image clear = makeCover(160, 160, 0);
    badge::apply(clear);
    CHECK(badge::isMarked(clear));

    // lossless round trip, and the badge survives JPEG compression
    std::vector<std::uint8_t> png, jpg;
    CHECK(badge::encode(marked, badge::Format::Png, png));
    CHECK_EQ(badge::sniff(png), badge::Format::Png);
    badge::Image back;
    CHECK(badge::decode(png, back));
    CHECK(back.rgba == marked.rgba);
    CHECK(badge::encode(marked, badge::Format::Jpeg, jpg));
    CHECK_EQ(badge::sniff(jpg), badge::Format::Jpeg);
    badge::Image jback;
    CHECK(badge::decode(jpg, jback));
    CHECK(badge::isMarked(jback));
    CHECK(!badge::encode(marked, badge::Format::Unknown, png));

    // erase rebuilds a smooth cover closely
    badge::Image restored = marked;
    badge::erase(restored);
    CHECK(!badge::isMarked(restored));
    CHECK(maxDiff(plain, restored) < 24);

    // sizes outside 32..4096 and garbage are refused
    badge::Image small = makeCover(31, 31);
    std::vector<std::uint8_t> smallPng;
    CHECK(badge::encode(small, badge::Format::Png, smallPng));
    badge::Image none;
    CHECK(!badge::decode(smallPng, none));
    CHECK(!badge::decode(Bytes{1, 2, 3, 4, 5, 6, 7, 8, 9}, none));
    CHECK_EQ(badge::sniff(Bytes{}), badge::Format::Unknown);
    // a non-square cover (wide ICON0) is marked inside its own top-left corner
    badge::Image wide = makeCover(320, 176);
    badge::apply(wide);
    CHECK(badge::isMarked(wide));
}

// --------------------------------------------------------------- library --

static std::string tempDir(const char* name) {
    std::string base = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/hometidy_ps3_" + name + "_" +
                       std::to_string(std::rand());
    if (std::system(("rm -rf '" + base + "'").c_str()) != 0) std::printf("warning: could not clean %s\n", base.c_str());
    fsx::makeDirs(base);
    return base;
}

static void touch(const std::string& path, const std::string& text = "x") {
    fsx::writeBytes(path, Bytes(text.begin(), text.end()));
}

static void testLibrary() {
    const std::string d = tempDir("library");
    touch(d + "/Alpha (USA) (En,Fr).iso");
    touch(d + "/Alpha (USA) (En,Fr).png");
    touch(d + "/Beta - The Return (USA).ISO");
    touch(d + "/Beta - The Return (USA).JPG");
    touch(d + "/Gamma (USA).iso");              // no cover
    touch(d + "/Delta (USA).iso");
    touch(d + "/Delta (USA).iso.jpg");          // webMAN style name
    touch(d + "/._Alpha (USA) (En,Fr).iso");    // macOS sidecar
    touch(d + "/Orphan cover.png");             // cover without game
    touch(d + "/Disc Game (Track 01).bin");
    touch(d + "/Disc Game (Track 02).bin");
    touch(d + "/Disc Game (Track 01).png");
    touch(d + "/notes.txt");
    const auto games = library::scan({{d, library::Platform::Ps2}, {d + "/missing", library::Platform::Psx}});
    CHECK_EQ(games.size(), size_t(5));
    auto find = [&](const std::string& name) -> const library::Game* {
        for (const auto& g : games) {
            if (g.name == name) return &g;
        }
        return nullptr;
    };
    const auto* alpha = find("Alpha (USA) (En,Fr)");
    CHECK(alpha != nullptr);
    if (alpha) {
        CHECK_EQ(alpha->title, std::string("Alpha"));
        CHECK_EQ(alpha->image, d + "/Alpha (USA) (En,Fr).png");
        CHECK(alpha->platform == library::Platform::Ps2);
    }
    const auto* beta = find("Beta - The Return (USA)");
    CHECK(beta != nullptr);
    if (beta) {
        CHECK_EQ(beta->title, std::string("Beta: The Return"));
        CHECK_EQ(beta->image, d + "/Beta - The Return (USA).JPG");
    }
    const auto* gamma = find("Gamma (USA)");
    CHECK(gamma != nullptr && gamma->image.empty());
    const auto* delta = find("Delta (USA)");
    CHECK(delta != nullptr && delta->image == d + "/Delta (USA).iso.jpg");
    CHECK(find("Disc Game (Track 02)") == nullptr);
    CHECK(find("Disc Game (Track 01)") != nullptr);
    CHECK_EQ(std::string(library::label(library::Platform::Psx)), std::string("PS1"));
}

// ----------------------------------------------------------------- tasks --

static void testSortTasks() {
    const std::string dir = tempDir("sort");
    const std::string path = dir + "/mygames.xml";
    const std::string xml = sampleXml({{"0001", "Resistance 3 (USA)"}, {"0002", "Resistance - Fall of Man (USA)"},
                                       {"0003", "Alpha Protocol (USA)"}},
                                      {"0001", "0002", "0003"});
    touch(path, xml);
    auto plan = tasks::planSort(xml, rules());
    CHECK(plan.has_value());
    if (!plan) return;
    CHECK_EQ(plan->changedSegments, 1);
    CHECK_EQ(plan->segments.size(), size_t(1));
    CHECK_EQ(plan->segments[0].label, std::string("PS3"));
    CHECK_EQ(plan->segments[0].rows[0].name, std::string("Alpha Protocol"));
    CHECK_EQ(plan->segments[0].rows[1].name, std::string("Resistance: Fall of Man"));
    CHECK_EQ(plan->segments[0].rows[2].from, 0);
    CHECK_EQ(plan->segments[0].moves(), 2);
    CHECK_EQ(plan->sorted.size(), xml.size());

    // someone changes the file after the plan: nothing is written
    touch(path, xml + " ");
    CHECK(tasks::applySort(path, *plan, dir + "/backup", "20260101_000000").status == tasks::Status::Changed);
    touch(path, xml);

    const auto r = tasks::applySort(path, *plan, dir + "/backup", "20260101_000000");
    CHECK(r.status == tasks::Status::Ok);
    CHECK(tasks::fileEquals(path, plan->sorted));
    CHECK(tasks::fileEquals(r.backupPath, xml));
    CHECK_EQ(r.backupPath, dir + "/backup/mygames_20260101_000000.xml");

    // a sorted file has nothing left to do
    std::string sortedNow;
    CHECK(fsx::readFile(path, sortedNow));
    const auto again = tasks::planSort(sortedNow, rules());
    CHECK(again.has_value() && again->changedSegments == 0);
    if (again) CHECK(tasks::applySort(path, *again, dir + "/backup", "x").status == tasks::Status::NothingToDo);
    CHECK(!tasks::planSort("<X></X>", rules()).has_value());
}

static void testCoverTasks() {
    const std::string dir = tempDir("cover");
    const std::string backup = dir + "/backup";
    std::vector<std::uint8_t> png, jpg;
    badge::encode(makeCover(), badge::Format::Png, png);
    badge::encode(makeCover(), badge::Format::Jpeg, jpg);
    fsx::writeBytes(dir + "/a.png", png);
    fsx::writeBytes(dir + "/b.jpg", jpg);
    touch(dir + "/bad.png", "not an image");

    CHECK(tasks::coverState(dir + "/a.png") == tasks::CoverState::Plain);
    CHECK(tasks::coverState(dir + "/bad.png") == tasks::CoverState::Unreadable);
    CHECK(tasks::coverState(dir + "/none.png") == tasks::CoverState::Unreadable);
    CHECK(tasks::markCover(dir + "/bad.png", backup).status == tasks::Status::Unsupported);
    CHECK(tasks::markCover(dir + "/none.png", backup).status == tasks::Status::ReadFailed);

    for (const char* name : {"a.png", "b.jpg"}) {
        const std::string path = dir + "/" + name;
        std::vector<std::uint8_t> before;
        CHECK(fsx::readBytes(path, before));
        const auto m = tasks::markCover(path, backup);
        CHECK(m.status == tasks::Status::Ok);
        CHECK(tasks::coverState(path) == tasks::CoverState::Marked);
        CHECK(fsx::exists(m.backupPath));
        CHECK(tasks::markCover(path, backup).status == tasks::Status::NothingToDo);
        const auto u = tasks::unmarkCover(path, backup);
        CHECK(u.status == tasks::Status::Ok && !u.approximate);
        std::vector<std::uint8_t> after;
        CHECK(fsx::readBytes(path, after));
        CHECK(after == before);  // the saved original comes back byte for byte
        CHECK(tasks::coverState(path) == tasks::CoverState::Plain);
    }

    // marked by another tool: no backup, the badge is rebuilt approximately
    const std::string other = dir + "/other.png";
    badge::Image m = makeCover();
    badge::apply(m);
    std::vector<std::uint8_t> otherPng;
    badge::encode(m, badge::Format::Png, otherPng);
    fsx::writeBytes(other, otherPng);
    CHECK(tasks::coverState(other) == tasks::CoverState::Marked);
    const auto u = tasks::unmarkCover(other, backup);
    CHECK(u.status == tasks::Status::Ok && u.approximate);
    CHECK(tasks::coverState(other) == tasks::CoverState::Plain);
    // nothing to remove on a plain cover without a backup
    CHECK(tasks::unmarkCover(other, backup).status == tasks::Status::NoBackup);
    // the backup keeps the first original even after a second mark
    const std::string again = dir + "/again.png";
    fsx::writeBytes(again, png);
    tasks::markCover(again, backup);
    tasks::unmarkCover(again, backup);
    std::vector<std::uint8_t> firstBackup;
    CHECK(fsx::readBytes(tasks::coverBackupPath(again, backup), firstBackup));
    CHECK(firstBackup == png);
}


// -------------------------------------------------------------------- gfx --

static std::vector<std::uint8_t> loadFont(bool bold) { return platform::fontBytes(bold); }

static std::uint32_t pixelAt(const gfx::Canvas& c, int x, int y) { return c.pixels()[std::size_t(y) * c.width() + x]; }


// ----------------------------------------------------------------- covers --

static Bytes bytesOf(const std::string& s) { return Bytes(s.begin(), s.end()); }

static void testSha1() {
    CHECK_EQ(sha1::hex(reinterpret_cast<const std::uint8_t*>("abc"), 3), std::string("a9993e364706816aba3e25717850c26c9cd0d89d"));
    CHECK_EQ(sha1::hex(nullptr, 0), std::string("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
    // 56 bytes: the padding spills into a second block
    const std::string two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK_EQ(sha1::hex(reinterpret_cast<const std::uint8_t*>(two.data()), two.size()), std::string("84983e441c3bd26ebaae4aa1f95129e5e54670f1"));
    // git blob ids, as printed by `git hash-object`
    CHECK_EQ(sha1::gitBlob(bytesOf("hello\n")), std::string("ce013625030ba8dba906f756967f9e9ca394464a"));
    CHECK_EQ(sha1::gitBlob({}), std::string("e69de29bb2d1d6434b8b29ae775ad8c2e48c5391"));
}

static const char* kRootTree =
    R"({"sha":"r","url":"u","tree":[{"path":"Named_Boxarts","mode":"040000","type":"tree","sha":"boxtree","url":"x"},)"
    R"({"path":"README.md","mode":"100644","type":"blob","sha":"aaa","size":10,"url":"y"}],"truncated":false})";

struct File {
    std::string name;
    std::string sha;
    std::string mode = "100644";
};

static std::string folderTree(const std::vector<File>& files) {
    std::string out = R"({"sha":"boxtree","tree":[)";
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (i) out += ",";
        out += R"({"path":")" + files[i].name + R"(","mode":")" + files[i].mode + R"(","type":"blob","sha":")" + files[i].sha + R"(","size":5,"url":"z"})";
    }
    return out + R"(],"truncated":false})";
}

static covers::Index indexOf(const std::vector<File>& files) {
    std::vector<covers::TreeItem> items;
    CHECK(covers::parseTree(folderTree(files), items));
    return covers::indexImages(items);
}

static void testCoverIndex() {
    std::vector<covers::TreeItem> items;
    CHECK(covers::parseTree(kRootTree, items));
    CHECK_EQ(items.size(), std::size_t(2));
    CHECK_EQ(items[0].mode, std::string("040000"));
    CHECK_EQ(covers::findSha(items, "Named_Boxarts", "tree"), std::string("boxtree"));
    CHECK_EQ(covers::findSha(items, "Named_Boxarts", "blob"), std::string(""));
    CHECK_EQ(covers::findSha(items, "nope", "tree"), std::string(""));

    CHECK(!covers::parseTree(R"({"tree":[],"truncated":true})", items));  // cut short: not trusted
    CHECK(!covers::parseTree("not json", items));
    CHECK(!covers::parseTree(R"({"sha":"x"})", items));  // no tree array
    CHECK(!covers::parseTree(R"({"tree":[{"path":"a","sha":)", items));
    CHECK(covers::parseTree(R"({"tree":[]})", items) && items.empty());

    // escapes: quote, backslash, é, a surrogate pair
    CHECK(covers::parseTree(R"({"tree":[{"path":"a\"b\\c é 😀.png","type":"blob","sha":"s1"}]})", items));
    CHECK_EQ(items[0].path, std::string("a\"b\\c \xC3\xA9 \xF0\x9F\x98\x80.png"));

    const auto index = indexOf({{"Radiata Stories (USA).png", "s1"}, {"Notes.txt", "s2"}, {"Ico_ (Japan).PNG", "s3"},
                                {"Alias (USA).png", "s4", "120000"}, {"Sub.png", "s5", "160000"}});
    CHECK_EQ(index.exact.size(), std::size_t(3));  // .png files and links only, any letter case; not a submodule
    CHECK(covers::find(index, "radiata stories (usa)") != nullptr);
    CHECK_EQ(covers::find(index, "Radiata Stories (USA)")->sha, std::string("s1"));
    CHECK(covers::find(index, "Radiata Stories (Europe)") == nullptr);
    CHECK(covers::find(index, "Alias (USA)")->symlink);
    // the repository writes "_" for forbidden characters; tags like [BLUS30443] are dropped as another try
    CHECK(covers::find(index, "Ico: (Japan)") != nullptr);
    CHECK(covers::find(index, "Radiata Stories (USA) [SLUS-21262]") != nullptr);

    // language lists, versions, region lists and revisions are dropped step by step
    const auto tiers = covers::candidateNames("Infamous (USA, Canada) (En,Fr,Es) (v02.00) (Rev 1)");
    CHECK(std::find(tiers.begin(), tiers.end(), "Infamous (USA, Canada) (Rev 1)") != tiers.end());
    CHECK(std::find(tiers.begin(), tiers.end(), "Infamous (USA) (Rev 1)") != tiers.end());
    CHECK(std::find(tiers.begin(), tiers.end(), "Infamous (USA)") != tiers.end());
    CHECK_EQ(tiers.front(), std::string("Infamous (USA, Canada) (En,Fr,Es) (v02.00) (Rev 1)"));
    const auto idx2 = indexOf({{"Killzone 2 (USA).png", "k2"}, {"Chrono Cross (USA) (Disc 1).png", "c1"},
                               {"Legend of Dragoon, The (USA) (Disc 2).png", "l2"}, {"Final Fantasy IX (USA) (Disc 1).png", "f9"}});
    CHECK_EQ(covers::find(idx2, "Killzone 2 (USA) (En,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi,Pl,Ru)")->sha, std::string("k2"));
    CHECK_EQ(covers::find(idx2, "Chrono Cross (USA, Canada) (Disc 1)")->sha, std::string("c1"));
    CHECK_EQ(covers::find(idx2, "Legend of Dragoon, The (USA, Canada) (Disc 2)")->sha, std::string("l2"));
    CHECK_EQ(covers::find(idx2, "Final Fantasy IX (USA, Canada) (Disc 1) (Rev 1)")->sha, std::string("f9"));
    CHECK(covers::find(idx2, "Chrono Cross (USA, Canada) (Disc 2)") == nullptr);  // a disc of its own

    // same title in another region: preferred USA, then World, then Europe; the disc must match
    const auto idx3 = indexOf({{"Tales of Xillia (Japan).png", "j"}, {"Tales of Xillia (Europe).png", "e"}, {"Tales of Xillia (World).png", "w"},
                               {"Tales & Co (Japan) (Disc 1).png", "d1"}});
    CHECK_EQ(covers::findLoose(idx3, "Tales of Xillia (USA) (En,Fr,Es)")->sha, std::string("w"));
    CHECK_EQ(covers::findLoose(idx3, "tales of xillia")->sha, std::string("w"));
    CHECK(covers::findLoose(idx3, "Tales of Xillia 2 (USA)") == nullptr);
    CHECK_EQ(covers::findLoose(idx3, "Tales & Co (USA) (Disc 1)")->sha, std::string("d1"));
    CHECK(covers::findLoose(idx3, "Tales & Co (USA) (Disc 2)") == nullptr);

    // links: the content is a file name of the same folder
    CHECK_EQ(covers::linkTarget("Some Game (USA).png"), std::string("Some Game (USA).png"));
    CHECK_EQ(covers::linkTarget("Some Game (USA).png\n"), std::string("Some Game (USA).png"));
    CHECK_EQ(covers::linkTarget("../Named_Snaps/x.png"), std::string(""));
    CHECK_EQ(covers::linkTarget("notes.txt"), std::string(""));
    CHECK_EQ(covers::findTarget(idx2, "killzone 2 (USA).png")->sha, std::string("k2"));
    CHECK(covers::findTarget(idx2, "Nope.png") == nullptr);

    const auto& ps2 = covers::sourceFor(library::Platform::Ps2);
    CHECK_EQ(std::string(ps2.repo), std::string("libretro-thumbnails/Sony_-_PlayStation_2"));
    CHECK_EQ(std::string(covers::sourceFor(library::Platform::Psx).repo), std::string("libretro-thumbnails/Sony_-_PlayStation"));
    CHECK_EQ(std::string(covers::sourceFor(library::Platform::Ps3).repo), std::string("libretro-thumbnails/Sony_-_PlayStation_3"));
    CHECK_EQ(covers::imageUrl(ps2, "Ico & Co (USA)"),
             std::string("https://raw.githubusercontent.com/libretro-thumbnails/Sony_-_PlayStation_2/master/Named_Boxarts/Ico%20%26%20Co%20%28USA%29.png"));
    CHECK_EQ(covers::treeUrl(ps2, "master"), std::string("https://api.github.com/repos/libretro-thumbnails/Sony_-_PlayStation_2/git/trees/master"));
    CHECK_EQ(covers::gametdbUrl("BLUS30341", "US"), std::string("https://art.gametdb.com/ps3/cover/US/BLUS30341.jpg"));
    CHECK_EQ(covers::gametdbRegions("BLUS30341").front(), std::string("US"));
    CHECK_EQ(covers::gametdbRegions("BLES01234").front(), std::string("EN"));
    CHECK_EQ(covers::gametdbRegions("BLJM60001").front(), std::string("JA"));
}

// ---- disc images -------------------------------------------------------

static Bytes sfoWith(const std::string& key, const std::string& value) {
    Bytes b(36, 0);
    auto put = [&](std::size_t at, std::uint32_t v, int bytes) {
        for (int i = 0; i < bytes; ++i) b[at + std::size_t(i)] = std::uint8_t(v >> (8 * i));
    };
    b[1] = 'P', b[2] = 'S', b[3] = 'F';
    put(4, 0x101, 4);                                                    // version 1.1
    put(8, 36, 4);                                                       // key table
    put(12, 36 + std::uint32_t(key.size()) + 1, 4);                      // data table
    put(16, 1, 4);                                                       // one entry
    put(20, 0, 2), put(22, 0x0204, 2);                                   // key offset, utf-8 string
    put(24, std::uint32_t(value.size()) + 1, 4), put(28, std::uint32_t(value.size()) + 1, 4), put(32, 0, 4);
    for (const char c : key) b.push_back(std::uint8_t(c));
    b.push_back(0);
    for (const char c : value) b.push_back(std::uint8_t(c));
    b.push_back(0);
    return b;
}

static Bytes dirRecord(const std::string& name, std::uint32_t lba, std::uint32_t size, bool dir) {
    const std::size_t len = (33 + name.size() + 1) & ~std::size_t(1);
    Bytes r(len, 0);
    r[0] = std::uint8_t(len);
    auto both = [&](std::size_t at, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            r[at + std::size_t(i)] = std::uint8_t(v >> (8 * i));
            r[at + 4 + std::size_t(3 - i)] = std::uint8_t(v >> (8 * i));
        }
    };
    both(2, lba), both(10, size);
    r[25] = dir ? 2 : 0;
    r[32] = std::uint8_t(name.size());
    for (std::size_t i = 0; i < name.size(); ++i) r[33 + i] = std::uint8_t(name[i]);
    return r;
}

// A minimal ISO 9660 image: /PS3_GAME/PARAM.SFO holding the title id (or no PS3_GAME at all).
static Bytes makeIso(const std::string& titleId, bool withGame = true) {
    Bytes iso(24 * 2048, 0);
    auto at = [&](std::size_t sector) { return sector * 2048; };
    iso[at(16)] = 1;
    std::memcpy(&iso[at(16) + 1], "CD001", 5);
    const Bytes root = dirRecord("", 20, 2048, true);
    std::copy(root.begin(), root.end(), iso.begin() + at(16) + 156);
    auto put = [&](std::size_t sector, std::vector<Bytes> recs) {
        std::size_t off = at(sector);
        for (const Bytes& r : recs) {
            std::copy(r.begin(), r.end(), iso.begin() + off);
            off += r.size();
        }
    };
    put(20, {dirRecord(".", 20, 2048, true), dirRecord("..", 20, 2048, true), dirRecord("PS3_DISC.SFB;1", 23, 4, false),
             withGame ? dirRecord("PS3_GAME", 21, 2048, true) : dirRecord("OTHER", 21, 2048, true)});
    const Bytes sfo = sfoWith("TITLE_ID", titleId);
    put(21, {dirRecord(".", 21, 2048, true), dirRecord("..", 20, 2048, true), dirRecord("PARAM.SFO;1", 22, std::uint32_t(sfo.size()), false)});
    std::copy(sfo.begin(), sfo.end(), iso.begin() + at(22));
    return iso;
}

static void testIsoInfo() {
    const std::string dir = tempDir("iso");
    fsx::writeBytes(dir + "/a.iso", makeIso("BLUS30341"));
    CHECK_EQ(isoinfo::ps3TitleId(dir + "/a.iso"), std::string("BLUS30341"));
    fsx::writeBytes(dir + "/nogame.iso", makeIso("BLUS30341", false));
    CHECK_EQ(isoinfo::ps3TitleId(dir + "/nogame.iso"), std::string(""));
    fsx::writeBytes(dir + "/bad.iso", makeIso("../x"));                     // it goes into a URL: only letters and digits
    CHECK_EQ(isoinfo::ps3TitleId(dir + "/bad.iso"), std::string(""));
    fsx::writeBytes(dir + "/short.iso", makeIso("AB"));
    CHECK_EQ(isoinfo::ps3TitleId(dir + "/short.iso"), std::string(""));
    touch(dir + "/junk.iso", std::string(40000, 'x'));
    CHECK_EQ(isoinfo::ps3TitleId(dir + "/junk.iso"), std::string(""));
    CHECK_EQ(isoinfo::ps3TitleId(dir + "/none.iso"), std::string(""));
    fsx::writeBytes(dir + "/tiny.iso", Bytes(100, 0));
    CHECK_EQ(isoinfo::ps3TitleId(dir + "/tiny.iso"), std::string(""));
}

static void testFrame() {
    Bytes png;
    badge::encode(makeCover(400, 600), badge::Format::Png, png);
    Bytes out;
    CHECK(frame::fit(png, 160, out));
    badge::Image img;
    CHECK(badge::decode(out, img));
    CHECK_EQ(img.w, 160);
    CHECK_EQ(img.h, 160);
    auto alpha = [&](int x, int y) { return img.rgba[(std::size_t(y) * 160 + x) * 4 + 3]; };
    CHECK_EQ(alpha(2, 80), 0);    // 400x600 fits as 107x160: transparent bands left and right
    CHECK_EQ(alpha(157, 80), 0);
    CHECK(alpha(80, 80) > 200);   // the picture in the middle
    CHECK(alpha(80, 2) > 200);    // full height
    // a picture that already fits is centered, not enlarged
    Bytes small, smallOut;
    badge::encode(makeCover(64, 64), badge::Format::Png, small);
    CHECK(frame::fit(small, 160, smallOut));
    badge::Image si;
    CHECK(badge::decode(smallOut, si));
    CHECK_EQ(si.w, 160);
    CHECK_EQ(si.rgba[(std::size_t(80) * 160 + 80) * 4 + 3], 255);
    CHECK_EQ(si.rgba[(std::size_t(10) * 160 + 10) * 4 + 3], 0);
    // jpeg in, png out
    Bytes jpg;
    badge::encode(makeCover(300, 200), badge::Format::Jpeg, jpg);
    CHECK(frame::fit(jpg, 160, out));
    CHECK(badge::sniff(out) == badge::Format::Png);
    // deterministic: the same input always gives the same bytes (the comparison with the local cover relies on it)
    Bytes again;
    CHECK(frame::fit(jpg, 160, again));
    CHECK(again == out);
    CHECK(!frame::fit(Bytes{1, 2, 3}, 160, out));
    CHECK(!frame::fit({}, 160, out));
}

// Serves canned answers; every transfer takes a few steps so the progress can be seen moving.
class FakeClient : public http::Client {
public:
    struct Route {
        Route(int s = 200, Bytes b = {}, bool f = false) : status(s), body(std::move(b)), fail(f) {}
        int status;
        Bytes body;
        bool fail;
        std::map<std::string, std::string> headers;  ///< Lowercase names, for a HEAD.
    };
    std::map<std::string, Route> routes;
    std::vector<std::string> requested;
    std::vector<long long> seen;  // `done` bytes seen while stepping

    void start(const std::string& url, std::size_t maxBytes) override {
        requested.push_back(url);
        head_ = false;
        max_ = maxBytes;
        steps_ = 0;
        current_ = routes.find(url);
        // an address with no route answers 404, like a file that does not exist
        state_ = current_ != routes.end() && current_->second.fail ? http::State::Failed : http::State::Running;
    }
    void startHead(const std::string& url) override {
        requested.push_back("HEAD " + url);
        steps_ = 0;
        max_ = 1024;
        current_ = routes.find(url);
        state_ = current_ != routes.end() && current_->second.fail ? http::State::Failed : http::State::Running;
        head_ = true;
    }
    std::string header(const std::string& name) const override {
        if (current_ == routes.end()) return "";
        const auto it = current_->second.headers.find(name);
        return it == current_->second.headers.end() ? "" : it->second;
    }
    http::State step() override {
        if (state_ != http::State::Running) return state_;
        if (head_ && ++steps_ >= 1) {
            head_ = false;
            state_ = http::State::Done;
            return state_;
        }
        if (++steps_ >= 3) {
            state_ = current_ != routes.end() && current_->second.body.size() > max_ ? http::State::Failed : http::State::Done;
        }
        seen.push_back(progress().done);
        return state_;
    }
    http::Progress progress() const override {
        if (current_ == routes.end()) return {};
        const long long n = static_cast<long long>(current_->second.body.size());
        return {n * std::min(steps_, 3) / 3, n};
    }
    int status() const override { return current_ == routes.end() ? 404 : current_->second.status; }
    std::string error() const override { return "fake failure"; }
    Bytes take() override { return current_ == routes.end() ? Bytes() : current_->second.body; }
    void cancel() override { state_ = http::State::Idle; ++cancels; }
    int cancels = 0;

private:
    std::map<std::string, Route>::iterator current_ = routes.end();
    std::size_t max_ = 0;
    int steps_ = 0;
    bool head_ = false;
    http::State state_ = http::State::Idle;
};

static void runSync(coversync::Sync& sync) {
    for (int i = 0; i < 100000 && !sync.done(); ++i) sync.step();
}

static library::Game gameAt(const std::string& dir, const std::string& name, library::Platform p, const char* ext = ".iso") {
    library::Game g;
    g.name = name;
    g.file = name + ext;
    g.title = name;
    g.platform = p;
    g.dir = dir;
    for (const char* e : {".png", ".jpg"}) {
        if (fsx::exists(dir + "/" + name + e)) {
            g.image = dir + "/" + name + e;
            break;
        }
    }
    return g;
}

static Bytes imageBytes(int w, int h, badge::Format f = badge::Format::Png, bool marked = false) {
    badge::Image img = makeCover(w, h);
    if (marked) badge::apply(img);
    Bytes out;
    badge::encode(img, f, out);
    return out;
}

static bool framed(const std::string& path) {
    Bytes b;
    badge::Image img;
    return fsx::readBytes(path, b) && badge::decode(b, img) && img.w == 160 && img.h == 160;
}

static void testCoverSync() {
    const std::string root = tempDir("sync");
    const std::string ps3 = root + "/PS3ISO", ps2 = root + "/PS2ISO", db = root + "/covers.db";
    fsx::makeDirs(ps3);
    fsx::makeDirs(ps2);

    const Bytes big = imageBytes(400, 600), big2 = imageBytes(300, 300), gtdb = imageBytes(120, 170, badge::Format::Jpeg);
    const Bytes real = imageBytes(500, 700), linkText = std::vector<std::uint8_t>{'R', 'e', 'a', 'l', ' ', 'I', 'm', 'a', 'g', 'e', ' ', '(', 'U', 'S', 'A', ')', '.', 'p', 'n', 'g'};

    // PS3
    fsx::writeBytes(ps3 + "/Missing Cover (USA).iso", {});                 // no cover yet
    fsx::writeBytes(ps3 + "/Different (USA).iso", {});                       // a cover that is not the repository's
    fsx::writeBytes(ps3 + "/Different (USA).png", bytesOf("old image"));
    fsx::writeBytes(ps3 + "/Finished (USA).iso", {});                        // marked as completed: overwritten too
    fsx::writeBytes(ps3 + "/Finished (USA).png", imageBytes(160, 160, badge::Format::Png, true));
    fsx::writeBytes(ps3 + "/Unknown (USA).iso", {});                         // nowhere to be found
    fsx::writeBytes(ps3 + "/Fallback Game (USA) (En,Fr).iso", makeIso("BLUS99999"));
    fsx::writeBytes(ps3 + "/Euro Game (Europe).iso", makeIso("BLES11111"));
    fsx::writeBytes(ps3 + "/Tiered Game (USA) (En,Fr,De) (v02.00).iso", {});  // the repository names it without the lists
    // PS2
    fsx::writeBytes(ps2 + "/Jpg Game (USA).iso", {});
    fsx::writeBytes(ps2 + "/Jpg Game (USA).jpg", bytesOf("old jpg"));
    fsx::writeBytes(ps2 + "/Corrupt (USA).iso", {});
    fsx::writeBytes(ps2 + "/Linked (Europe).iso", {});                       // a name that is a link to another image
    fsx::writeBytes(ps2 + "/Loose Game (USA).iso", {});                      // only the Japan cover exists

    const covers::Source& s3 = covers::sourceFor(library::Platform::Ps3);
    const covers::Source& s2 = covers::sourceFor(library::Platform::Ps2);
    FakeClient fake;
    auto ok = [](const Bytes& body) { return FakeClient::Route{200, body, false}; };
    auto okText = [&](const std::string& body) { return ok(bytesOf(body)); };
    fake.routes[covers::treeUrl(s3, "master")] = okText(kRootTree);
    fake.routes[covers::treeUrl(s3, "boxtree")] =
        okText(folderTree({{"Missing Cover (USA).png", sha1::gitBlob(big)}, {"Different (USA).png", sha1::gitBlob(big2)},
                           {"Finished (USA).png", sha1::gitBlob(big)}, {"Tiered Game (USA).png", sha1::gitBlob(big2)}}));
    fake.routes[covers::imageUrl(s3, "Missing Cover (USA)")] = ok(big);
    fake.routes[covers::imageUrl(s3, "Different (USA)")] = ok(big2);
    fake.routes[covers::imageUrl(s3, "Finished (USA)")] = ok(big);
    fake.routes[covers::imageUrl(s3, "Tiered Game (USA)")] = ok(big2);
    auto withEtag = [&](const std::string& etag) {
        FakeClient::Route r = ok(gtdb);
        r.headers = {{"etag", etag}, {"content-length", std::to_string(gtdb.size())}};
        return r;
    };
    fake.routes[covers::gametdbUrl("BLUS99999", "US")] = withEtag("\"v1\"");
    fake.routes[covers::gametdbUrl("BLES11111", "EN")] = withEtag("\"e1\"");  // the US folder has no route: 404, then the next region
    fake.routes[covers::treeUrl(s2, "master")] = okText(kRootTree);
    fake.routes[covers::treeUrl(s2, "boxtree")] =
        okText(folderTree({{"Jpg Game (USA).png", sha1::gitBlob(big)}, {"Corrupt (USA).png", sha1::gitBlob(big)},
                           {"Linked (Europe).png", sha1::gitBlob(linkText), "120000"}, {"Real Image (USA).png", sha1::gitBlob(real)},
                           {"Loose Game (Japan).png", sha1::gitBlob(big2)}}));
    fake.routes[covers::imageUrl(s2, "Jpg Game (USA)")] = ok(big);
    fake.routes[covers::imageUrl(s2, "Corrupt (USA)")] = okText("tampered bytes!!");
    fake.routes[covers::imageUrl(s2, "Linked (Europe)")] = ok(linkText);
    fake.routes[covers::imageUrl(s2, "Real Image (USA)")] = ok(real);
    fake.routes[covers::imageUrl(s2, "Loose Game (Japan)")] = ok(big2);

    auto gamesNow = [&]() {
        std::vector<library::Game> g;
        for (const char* n : {"Missing Cover (USA)", "Different (USA)", "Finished (USA)", "Unknown (USA)", "Fallback Game (USA) (En,Fr)",
                              "Euro Game (Europe)", "Tiered Game (USA) (En,Fr,De) (v02.00)"}) {
            g.push_back(gameAt(ps3, n, library::Platform::Ps3));
        }
        for (const char* n : {"Jpg Game (USA)", "Corrupt (USA)", "Linked (Europe)", "Loose Game (USA)"}) g.push_back(gameAt(ps2, n, library::Platform::Ps2));
        return g;
    };

    coversync::Sync sync(fake, gamesNow(), db);
    CHECK(!sync.done());
    CHECK_EQ(sync.progress().total, 11);
    bool sawDownloading = false, sawName = false;
    for (int i = 0; i < 100000 && !sync.done(); ++i) {
        sync.step();
        if (sync.progress().phase == coversync::Phase::Downloading) {
            sawDownloading = true;
            sawName |= sync.progress().file == "Missing Cover (USA).png";
        }
    }
    CHECK(sync.done());
    CHECK(sawDownloading && sawName);
    const auto& c = sync.progress().counts;
    CHECK_EQ(c.added, 6);     // Missing, Fallback, Euro, Tiered, Linked, Loose
    CHECK_EQ(c.replaced, 3);  // Different, Finished (even with its mark), Jpg Game
    CHECK_EQ(c.same, 0);
    CHECK_EQ(c.missing, 1);   // Unknown
    CHECK_EQ(c.failed, 1);    // Corrupt
    CHECK_EQ(c.fallback, 2);  // the two PS3 games served by the fallback server
    CHECK_EQ(sync.progress().index, 11);
    CHECK_EQ(sync.progress().indexErrors, 0);

    // every written cover is the frame of the game list, not the big repository image
    for (const char* n : {"Missing Cover (USA)", "Different (USA)", "Finished (USA)", "Fallback Game (USA) (En,Fr)", "Euro Game (Europe)",
                          "Tiered Game (USA) (En,Fr,De) (v02.00)"}) {
        CHECK(framed(ps3 + "/" + n + ".png"));
    }
    for (const char* n : {"Jpg Game (USA)", "Linked (Europe)", "Loose Game (USA)"}) CHECK(framed(ps2 + "/" + n + ".png"));
    CHECK(fsx::exists(ps2 + "/Jpg Game (USA).jpg"));                // the old file is not deleted
    CHECK(!fsx::exists(ps2 + "/Corrupt (USA).png"));                // not written: the id did not match
    CHECK(!fsx::exists(ps2 + "/Corrupt (USA).png.part"));
    CHECK(!fsx::exists(ps3 + "/Unknown (USA).png"));
    Bytes got;
    badge::Image img;
    CHECK(fsx::readBytes(ps3 + "/Finished (USA).png", got) && badge::decode(got, img) && !badge::isMarked(img));  // the mark is gone
    CHECK(!sync.progress().lastError.empty());
    CHECK(std::any_of(fake.seen.begin(), fake.seen.end(), [](long long n) { return n > 0; }));
    CHECK(fsx::exists(db));

    // a second run: what the first run wrote is recognized, only the fallback server is asked again
    fake.requested.clear();
    coversync::Sync second(fake, gamesNow(), db);
    runSync(second);
    const auto& c2 = second.progress().counts;
    CHECK_EQ(c2.added, 0);
    CHECK_EQ(c2.replaced, 0);
    CHECK_EQ(c2.failed, 1);   // Corrupt, still
    CHECK_EQ(c2.missing, 1);
    CHECK_EQ(c2.same, 9);
    // nothing is downloaded again: only the cover lists and HEAD requests to the fallback server (Corrupt never
    // matched, so it is asked again, and it is the only image address that appears)
    for (const std::string& url : fake.requested) {
        const bool list = url.find("api.github.com") != std::string::npos;
        const bool head = url.compare(0, 5, "HEAD ") == 0;
        CHECK(list || head || url.find("Corrupt") != std::string::npos);
    }

    // a cover changed afterwards (the completed mark drawn again) no longer matches what was written: overwritten
    badge::Image again;
    CHECK(fsx::readBytes(ps3 + "/Missing Cover (USA).png", got) && badge::decode(got, again));
    badge::apply(again);
    Bytes markedPng;
    badge::encode(again, badge::Format::Png, markedPng);
    fsx::writeBytes(ps3 + "/Missing Cover (USA).png", markedPng);
    coversync::Sync third(fake, gamesNow(), db);
    runSync(third);
    CHECK_EQ(third.progress().counts.replaced, 1);
    CHECK(fsx::readBytes(ps3 + "/Missing Cover (USA).png", got) && badge::decode(got, again) && !badge::isMarked(again));

    // the repository image changed: the id differs from the recorded one and the cover is fetched again
    const Bytes newer = imageBytes(320, 320);
    fake.routes[covers::treeUrl(s3, "boxtree")] =
        okText(folderTree({{"Missing Cover (USA).png", sha1::gitBlob(newer)}, {"Different (USA).png", sha1::gitBlob(big2)},
                           {"Finished (USA).png", sha1::gitBlob(big)}, {"Tiered Game (USA).png", sha1::gitBlob(big2)}}));
    fake.routes[covers::imageUrl(s3, "Missing Cover (USA)")] = ok(newer);
    coversync::Sync fourth(fake, gamesNow(), db);
    runSync(fourth);
    CHECK_EQ(fourth.progress().counts.replaced, 1);

    // the fallback server's image changed (new ETag): fetched again; unchanged ones are not
    fake.routes[covers::gametdbUrl("BLUS99999", "US")] = withEtag("\"v2\"");
    fake.requested.clear();
    coversync::Sync fifth(fake, gamesNow(), db);
    runSync(fifth);
    CHECK_EQ(fifth.progress().counts.replaced, 0);   // the same bytes came back: the file is already this one
    CHECK(std::count(fake.requested.begin(), fake.requested.end(), covers::gametdbUrl("BLUS99999", "US")) == 1);
    CHECK(std::count(fake.requested.begin(), fake.requested.end(), covers::gametdbUrl("BLES11111", "EN")) == 0);
    CHECK_EQ(fifth.progress().counts.fallback, 0);
}

static void testCoverSyncSkipsWhatIsHere() {
    const std::string root = tempDir("syncskip");
    const std::string dir = root + "/PS2ISO";
    fsx::makeDirs(dir);
    const covers::Source& s2 = covers::sourceFor(library::Platform::Ps2);
    const Bytes real = imageBytes(500, 700);
    // v1.1.0 left the repository image as it is on disk: the same bytes as the remote
    fsx::writeBytes(dir + "/Raw Copy (USA).iso", {});
    fsx::writeBytes(dir + "/Raw Copy (USA).png", real);
    FakeClient fake;
    fake.routes[covers::treeUrl(s2, "master")] = {200, bytesOf(kRootTree), false};
    fake.routes[covers::treeUrl(s2, "boxtree")] = {200, bytesOf(folderTree({{"Raw Copy (USA).png", sha1::gitBlob(real)}})), false};
    // no route for the image itself: a download would be a 404 and a failure
    coversync::Sync sync(fake, {gameAt(dir, "Raw Copy (USA)", library::Platform::Ps2)}, root + "/covers.db");
    runSync(sync);
    CHECK_EQ(sync.progress().counts.failed, 0);
    CHECK_EQ(sync.progress().counts.replaced, 1);    // shrunk in place
    CHECK_EQ(fake.requested.size(), std::size_t(2));  // the two lists, no image
    CHECK(framed(dir + "/Raw Copy (USA).png"));
    // and the next run knows it is current
    fake.requested.clear();
    coversync::Sync again(fake, {gameAt(dir, "Raw Copy (USA)", library::Platform::Ps2)}, root + "/covers.db");
    runSync(again);
    CHECK_EQ(again.progress().counts.same, 1);
    CHECK_EQ(fake.requested.size(), std::size_t(2));
}

static void testCoverSyncFailures() {
    const std::string root = tempDir("syncfail");
    fsx::makeDirs(root + "/PS2ISO");
    fsx::makeDirs(root + "/PSXISO");
    fsx::writeBytes(root + "/PS2ISO/A (USA).iso", {});
    fsx::writeBytes(root + "/PSXISO/B (USA).bin", {});
    std::vector<library::Game> games = {gameAt(root + "/PS2ISO", "A (USA)", library::Platform::Ps2),
                                        gameAt(root + "/PSXISO", "B (USA)", library::Platform::Psx, ".bin")};
    auto text = [](const std::string& s) { return FakeClient::Route{200, bytesOf(s), false}; };

    {  // offline: every list fails, the run still ends and says so
        FakeClient fake;
        fake.routes[covers::treeUrl(covers::sourceFor(library::Platform::Ps2), "master")].fail = true;
        fake.routes[covers::treeUrl(covers::sourceFor(library::Platform::Psx), "master")].fail = true;
        coversync::Sync sync(fake, games);
        runSync(sync);
        CHECK(sync.done());
        CHECK_EQ(sync.progress().indexErrors, 2);
        CHECK_EQ(sync.progress().counts.failed, 2);
        CHECK_EQ(sync.progress().index, 2);
    }
    {  // rate limited (403) on the first repository, fine on the second
        FakeClient fake;
        const covers::Source& s2 = covers::sourceFor(library::Platform::Ps2);
        const covers::Source& s1 = covers::sourceFor(library::Platform::Psx);
        fake.routes[covers::treeUrl(s2, "master")] = {403, bytesOf("{\"message\":\"rate limit\"}"), false};
        fake.routes[covers::treeUrl(s1, "master")] = text(kRootTree);
        fake.routes[covers::treeUrl(s1, "boxtree")] = text(folderTree({}));
        coversync::Sync sync(fake, games);
        runSync(sync);
        CHECK_EQ(sync.progress().indexErrors, 1);
        CHECK_EQ(sync.progress().counts.failed, 1);
        CHECK_EQ(sync.progress().counts.missing, 1);
        CHECK(sync.progress().lastError.find("403") != std::string::npos);
    }
    {  // a list the repository cut short is refused
        FakeClient fake;
        const covers::Source& s2 = covers::sourceFor(library::Platform::Ps2);
        fake.routes[covers::treeUrl(s2, "master")] = text(kRootTree);
        fake.routes[covers::treeUrl(s2, "boxtree")] = text(R"({"tree":[],"truncated":true})");
        coversync::Sync sync(fake, {games[0]});
        runSync(sync);
        CHECK_EQ(sync.progress().indexErrors, 1);
    }
    {  // PS3 with the repository down: the fallback server still serves the cover
        const std::string ps3 = root + "/PS3ISO";
        fsx::makeDirs(ps3);
        fsx::writeBytes(ps3 + "/G (USA).iso", makeIso("BLUS12345"));
        FakeClient fake;
        fake.routes[covers::treeUrl(covers::sourceFor(library::Platform::Ps3), "master")] = {500, {}, false};
        fake.routes[covers::gametdbUrl("BLUS12345", "US")] = {200, imageBytes(100, 140, badge::Format::Jpeg), false};
        coversync::Sync sync(fake, {gameAt(ps3, "G (USA)", library::Platform::Ps3)});
        runSync(sync);
        CHECK_EQ(sync.progress().indexErrors, 1);
        CHECK_EQ(sync.progress().counts.added, 1);
        CHECK_EQ(sync.progress().counts.fallback, 1);
        CHECK(framed(ps3 + "/G (USA).png"));
    }
    {  // a link that points nowhere, and a link loop
        const std::string dir = root + "/PS2ISO";
        const Bytes loopText = bytesOf("Loop B (USA).png"), loopBack = bytesOf("Loop A (USA).png");
        fsx::writeBytes(dir + "/Loop A (USA).iso", {});
        fsx::writeBytes(dir + "/Dead (USA).iso", {});
        FakeClient fake;
        const covers::Source& s2 = covers::sourceFor(library::Platform::Ps2);
        fake.routes[covers::treeUrl(s2, "master")] = text(kRootTree);
        fake.routes[covers::treeUrl(s2, "boxtree")] =
            text(folderTree({{"Loop A (USA).png", sha1::gitBlob(loopText), "120000"}, {"Loop B (USA).png", sha1::gitBlob(loopBack), "120000"},
                             {"Dead (USA).png", sha1::gitBlob(bytesOf("Gone (USA).png")), "120000"}}));
        fake.routes[covers::imageUrl(s2, "Loop A (USA)")] = {200, loopText, false};
        fake.routes[covers::imageUrl(s2, "Loop B (USA)")] = {200, loopBack, false};
        fake.routes[covers::imageUrl(s2, "Dead (USA)")] = {200, bytesOf("Gone (USA).png"), false};
        coversync::Sync sync(fake, {gameAt(dir, "Loop A (USA)", library::Platform::Ps2), gameAt(dir, "Dead (USA)", library::Platform::Ps2)});
        runSync(sync);
        CHECK(sync.done());
        CHECK_EQ(sync.progress().counts.failed, 2);
        CHECK_EQ(sync.progress().counts.added, 0);
    }
    {  // cancel: stops at once, tells the client and keeps what was done
        FakeClient fake;
        coversync::Sync sync(fake, games);
        sync.step();
        sync.cancel();
        CHECK(sync.done() && sync.progress().cancelled);
        CHECK_EQ(fake.cancels, 1);
    }
}

static void testGfx() {
    gfx::Canvas c(1280, 720, loadFont(false), loadFont(true));
    CHECK(c.hasFont());
    c.clear({10, 20, 30});
    CHECK_EQ(pixelAt(c, 5, 5), 0xFF0A141Eu);
    c.fill(100, 100, 200, 100, 20, {255, 0, 0});
    CHECK_EQ(pixelAt(c, 200, 150), 0xFFFF0000u);   // solid inside
    CHECK_EQ(pixelAt(c, 100, 100), 0xFF0A141Eu);   // rounded corner leaves the background
    c.fill(400, 100, 100, 100, 0, {0, 255, 0, 128});
    const std::uint32_t half = pixelAt(c, 450, 150);
    CHECK(((half >> 8) & 0xFF) > 120 && ((half >> 8) & 0xFF) < 150);  // 50% alpha over the background
    c.circle(640, 400, 50, {0, 0, 255});
    CHECK_EQ(pixelAt(c, 640, 400), 0xFF0000FFu);
    CHECK_EQ(pixelAt(c, 640, 330), 0xFF0A141Eu);
    c.clear({0, 0, 0});
    c.stroke(100, 100, 100, 100, 10, 6, {255, 255, 255});
    CHECK_EQ(pixelAt(c, 150, 150), 0xFF000000u);   // the inside stays empty
    CHECK(pixelAt(c, 150, 102) != 0xFF000000u);    // the border is painted

    // thumbnails: box filter keeps colour and alpha, and a 1:1 blit matches the size
    {
        const badge::Image im = makeCover(300, 200);
        const auto th = gfx::Canvas::thumbnail(im.rgba, im.w, im.h, 124, 124);
        CHECK(th && th->w == 124 && th->h == 124);
        if (th) {
            const std::uint32_t mid = th->px[62 * 124 + 62];
            CHECK_EQ(mid >> 24, 255u);
            CHECK(std::abs(int((mid >> 16) & 0xFF) - (40 + 150 * 120 / 300)) < 6);
            c.clear({0, 0, 0});
            c.blit(*th, 20, 20, 124, 124, 8);
            CHECK_EQ(pixelAt(c, 20 + 62, 20 + 62), mid);  // copied, not resampled
            CHECK_EQ(pixelAt(c, 20, 20), 0xFF000000u);     // rounded corner masked
        }
        CHECK(!gfx::Canvas::thumbnail(im.rgba, 0, 0, 124, 124));
    }

    // text
    CHECK(c.textWidth("HomeTidy", gfx::Size::Body) > 80);
    CHECK(c.textWidth("HomeTidy", gfx::Size::Large) > c.textWidth("HomeTidy", gfx::Size::Body));
    CHECK_EQ(c.textWidth("", gfx::Size::Body), 0);
    CHECK(c.textWidth("a\xC3\xA7\xC3\xA3o \xE2\x80\xA6", gfx::Size::Body) > 0);   // accents and the ellipsis exist in the font
    c.clear({0, 0, 0});
    const int drawn = c.text("Hello", gfx::Size::Body, {255, 255, 255}, 100, 100);
    CHECK(drawn > 40);
    bool lit = false;
    for (int y = 100; y < 135 && !lit; ++y) {
        for (int x = 100; x < 100 + drawn && !lit; ++x) lit = pixelAt(c, x, y) != 0xFF000000u;
    }
    CHECK(lit);
    // a limit shortens the text with an ellipsis, right alignment ends at x
    const int limited = c.text("A very long title that cannot fit", gfx::Size::Body, {255, 255, 255}, 100, 200, gfx::Align::Left, 150);
    CHECK(limited <= 150);
    const auto lines = c.wrap("one two three four five six seven eight", gfx::Size::Body, 200);
    CHECK(lines.size() > 1);
    for (const auto& l : lines) CHECK(c.textWidth(l, gfx::Size::Body) <= 200);
    CHECK(c.lineHeight(gfx::Size::Body) > 26);

    // textures: opaque and transparent pixels
    std::vector<std::uint8_t> rgba(4 * 4 * 4, 0);
    for (int i = 0; i < 8; ++i) {  // left half opaque red
        rgba[std::size_t(i / 2) * 16 + (i % 2) * 4] = 255;
        rgba[std::size_t(i / 2) * 16 + (i % 2) * 4 + 3] = 255;
    }
    const auto tex = gfx::Canvas::texture(rgba, 4, 4);
    CHECK(tex != nullptr);
    c.clear({0, 0, 255});
    c.blit(*tex, 0, 0, 40, 40);
    CHECK_EQ(pixelAt(c, 5, 20), 0xFFFF0000u);
    CHECK_EQ(pixelAt(c, 35, 20), 0xFF0000FFu);
    CHECK(gfx::Canvas::texture({}, 0, 0) == nullptr);

    // other output sizes keep the 16:9 layout centred
    gfx::Canvas hd(1920, 1080, loadFont(false), loadFont(true));
    hd.fill(0, 0, 1280, 720, 0, {255, 0, 0});
    CHECK_EQ(pixelAt(hd, 1919, 1079), 0xFFFF0000u);
    gfx::Canvas sd(720, 480, loadFont(false), loadFont(true));
    sd.clear({0, 0, 255});
    sd.fill(0, 0, 1280, 720, 0, {255, 0, 0});
    CHECK_EQ(pixelAt(sd, 360, 240), 0xFFFF0000u);
    CHECK_EQ(pixelAt(sd, 360, 5), 0xFF0000FFu);  // letterbox bar
}

// -------------------------------------------------------------------- app --

static platform::Input press(char k) {
    platform::Input i;
    switch (k) {
        case 'A': i.a = true; break;
        case 'B': i.b = true; break;
        case 'R': i.r = true; break;
        case 'r': i.right = true; break;
        case 'l': i.left = true; break;
        default: break;
    }
    return i;
}

static void testApp() {
    const std::string root = tempDir("app");
    const std::string hdd = root + "/dev_hdd0";
    fsx::makeDirs(hdd + "/xmlhost/game_plugin");
    fsx::makeDirs(hdd + "/PS3ISO");
    const std::vector<std::pair<std::string, std::string>> games = {
        {"0001", "Resistance 3 (USA)"}, {"0002", "Resistance - Fall of Man (USA)"}, {"0003", "Alpha Protocol (USA)"}};
    const std::string xml = sampleXml(games, {"0001", "0002", "0003"});
    touch(hdd + "/xmlhost/game_plugin/mygames.xml", xml);
    Bytes cover;
    badge::encode(makeCover(), badge::Format::Png, cover);
    for (const auto& g : games) {
        touch(hdd + "/PS3ISO/" + g.second + ".iso");
        if (g.first != "0002") fsx::writeBytes(hdd + "/PS3ISO/" + g.second + ".png", cover);  // one game has no cover
    }

    platform::Env env;
    env.appDir = hdd + "/hometidy";
    env.xmlPath = hdd + "/xmlhost/game_plugin/mygames.xml";
    env.roots = {{hdd + "/PS3ISO", library::Platform::Ps3}};
    env.storageOk = true;
    gfx::Canvas canvas(1280, 720, loadFont(false), loadFont(true));
    App app(canvas, env);
    for (int i = 0; i < 6; ++i) {
        app.update({});
        app.draw();
    }
    CHECK(app.screen() == App::Screen::Home);
    CHECK(app.takeDirty());

    // sort: preview, confirm, apply
    app.update(press('A'));
    CHECK(app.screen() == App::Screen::SortPreview);
    app.draw();
    app.update(press('A'));
    CHECK(app.screen() == App::Screen::SortConfirm);
    app.draw();
    app.update(press('B'));
    CHECK(app.screen() == App::Screen::SortPreview);
    app.update(press('A'));
    app.update(press('A'));          // confirm: the job runs after a few frames
    for (int i = 0; i < 4; ++i) {
        app.update({});
        app.draw();
    }
    CHECK(app.screen() == App::Screen::SortDone);
    app.draw();
    std::string now;
    CHECK(fsx::readFile(env.xmlPath, now));
    const auto sorted = mygames::List::parse(now);
    CHECK(sorted.has_value());
    if (sorted) {
        CHECK_EQ(sorted->segments()[0].games[0].key, std::string("0003"));  // Alpha Protocol
        CHECK_EQ(sorted->segments()[0].games[1].key, std::string("0002"));  // Resistance: Fall of Man
        CHECK_EQ(sorted->segments()[0].games[2].key, std::string("0001"));  // Resistance 3
    }
    CHECK_EQ(fsx::listDir(env.appDir + "/backup").size(), size_t(1));

    // mark: the second game has no cover, the first one does
    app.update(press('B'));
    CHECK(app.screen() == App::Screen::Home);
    app.update(press('r'));
    app.update(press('A'));
    CHECK(app.screen() == App::Screen::Mark);
    app.takeDirty();
    app.draw();
    CHECK(app.takeDirty());   // covers load one per frame: the first draw leaves one pending
    app.draw();
    app.draw();
    CHECK(!app.takeDirty());  // all loaded, no more redraws requested
    const std::string coverPath = hdd + "/PS3ISO/Alpha Protocol (USA).png";  // first in order
    app.update(press('A'));
    CHECK(tasks::coverState(coverPath) == tasks::CoverState::Marked);
    app.draw();
    app.update(press('A'));
    CHECK(tasks::coverState(coverPath) == tasks::CoverState::Plain);
    Bytes after;
    CHECK(fsx::readBytes(coverPath, after));
    CHECK(after == cover);
    platform::Input right;
    right.right = true;
    app.update(right);  // game without cover: a toast, nothing written
    app.update(press('A'));
    app.draw();
    CHECK(!fsx::exists(hdd + "/PS3ISO/Resistance - Fall of Man (USA).png"));
    app.update(press('B'));
    CHECK(app.screen() == App::Screen::Home);

    // a missing game list gives a message instead of a crash
    fsx::removeFile(env.xmlPath);
    app.update(press('l'));   // focus back on "Sort game list"
    app.update(press('A'));
    CHECK(app.screen() == App::Screen::Message);
    app.draw();
}

static void testCoversScreen() {
    const std::string root = tempDir("covscreen");
    const std::string hdd = root + "/dev_hdd0";
    fsx::makeDirs(hdd + "/PS3ISO");
    touch(hdd + "/PS3ISO/Resistance 3 (USA).iso");
    platform::Env env;
    env.appDir = hdd + "/hometidy";
    env.xmlPath = hdd + "/none.xml";
    env.roots = {{hdd + "/PS3ISO", library::Platform::Ps3}};
    env.storageOk = true;
    gfx::Canvas canvas(1280, 720, loadFont(false), loadFont(true));
    App app(canvas, env);
    for (int i = 0; i < 6; ++i) {
        app.update({});
        app.draw();
    }
    app.update(press('r'));  // to the second card
    app.update(press('r'));  // to the third: Update covers
    app.update(press('A'));
    CHECK(app.screen() == App::Screen::Covers);
    app.draw();
    app.update(press('A'));  // start: the host client is offline, so the run ends with a failure
    for (int i = 0; i < 20; ++i) {
        app.update({});
        app.draw();
    }
    CHECK(app.screen() == App::Screen::Covers);
    app.update(press('A'));  // back from the summary
    CHECK(app.screen() == App::Screen::Home);
}

int main() {
    testNormalize();
    testCleanTitle();
    testRules();
    testSeriesOrder();
    testFinalFantasyTactics();
    testTombRaiderOrder();
    testPs1SequelsWithoutNumbers();
    testUserLibrary();
    testBalanced();
    testMygames();
    testRealXml();
    testBadge();
    testLibrary();
    testSortTasks();
    testCoverTasks();
    testSha1();
    testCoverIndex();
    testIsoInfo();
    testFrame();
    testCoverSync();
    testCoverSyncSkipsWhatIsHere();
    testCoverSyncFailures();
    testGfx();
    testApp();
    testCoversScreen();
    if (g_failed) {
        std::printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
