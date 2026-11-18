#include "default_rules.h"

// Release dates (YYYY-MM, North America, the platform of the listed edition) come from memory and only
// decide the order inside a series, so a month that is a little off does not matter. Titles that match
// nothing are grouped by the text before ":" / " - " and ordered by their trailing number, which is
// enough for most numbered sequels. Edit /dev_hdd0/hometidy/series.txt to override or extend this table.
// Patterns match whole words, so "god of war ii" does not match "god of war iii".
const char* kDefaultRules = R"RULES(
# pattern | series | date           (first match wins; patterns are normalized)
# --- Sony first party (PS3)
uncharted drakes fortune        | uncharted | 2007-11
uncharted among thieves         | uncharted | 2009-10
uncharted 2                     | uncharted | 2009-10
uncharted drakes deception      | uncharted | 2011-11
uncharted 3                     | uncharted | 2011-11
god of war chains of olympus    | god of war | 2008-03
god of war ghost of sparta      | god of war | 2010-11
god of war collection           | god of war | 2009-11
god of war origins collection   | god of war | 2011-09
god of war saga                 | god of war | 2012-08
god of war ascension            | god of war | 2013-03
god of war iii                  | god of war | 2010-03
god of war ii                   | god of war | 2007-03
god of war                      | god of war | 2005-03
killzone 2                      | killzone | 2009-02
killzone 3                      | killzone | 2011-02
killzone trilogy                | killzone | 2013-11
killzone liberation             | killzone | 2006-10
killzone                        | killzone | 2004-11
resistance fall of man          | resistance | 2006-11
resistance retribution          | resistance | 2009-03
resistance 2                    | resistance | 2008-11
resistance 3                    | resistance | 2011-09
infamous festival of blood      | infamous | 2011-10
infamous 2                      | infamous | 2011-06
infamous                        | infamous | 2009-05
littlebigplanet karting         | littlebigplanet | 2012-11
littlebigplanet 2               | littlebigplanet | 2011-01
littlebigplanet 3               | littlebigplanet | 2014-11
littlebigplanet                 | littlebigplanet | 2008-10
ratchet & clank going commando  | ratchet & clank | 2003-11
ratchet & clank up your arsenal | ratchet & clank | 2004-11
ratchet & clank deadlocked      | ratchet & clank | 2005-10
ratchet & clank future tools of destruction | ratchet & clank | 2007-10
ratchet & clank future quest for booty      | ratchet & clank | 2008-08
ratchet & clank future a crack in time      | ratchet & clank | 2009-10
ratchet & clank all 4 one       | ratchet & clank | 2011-10
ratchet & clank collection      | ratchet & clank | 2012-06
ratchet & clank full frontal assault | ratchet & clank | 2012-11
ratchet & clank qforce          | ratchet & clank | 2012-11
ratchet & clank into the nexus  | ratchet & clank | 2013-11
ratchet & clank                 | ratchet & clank | 2002-11
sly collection                  | sly cooper | 2010-11
sly cooper thieves in time      | sly cooper | 2013-02
sly cooper                      | sly cooper | 2002-09
jak and daxter collection       | jak and daxter | 2012-02
jak and daxter the precursor legacy | jak and daxter | 2001-12
jak ii                          | jak and daxter | 2003-10
jak 3                           | jak and daxter | 2004-11
jak x                           | jak and daxter | 2005-10
jak and daxter                  | jak and daxter | 2001-12
heavy rain                      | heavy rain | 2010-02
beyond two souls                | beyond two souls | 2013-10
the last of us                  | last of us | 2013-06
puppeteer                       | puppeteer | 2013-09
playstation all stars           | playstation all stars | 2012-11
# --- Square Enix and JRPG
dirge of cerberus               | dirge of cerberus | 2006-08
final fantasy tactics           | final fantasy tactics | 1998-01  # not a numbered Final Fantasy: its own title
final fantasy vii               | final fantasy | 1997-09
final fantasy viii              | final fantasy | 1999-09
final fantasy ix                | final fantasy | 2000-11
final fantasy x 2               | final fantasy | 2003-11
final fantasy x x 2 hd          | final fantasy | 2014-03
final fantasy x                 | final fantasy | 2001-12
final fantasy xii               | final fantasy | 2006-10
final fantasy xiii 2            | final fantasy | 2012-01
final fantasy xiii              | final fantasy | 2010-03
lightning returns               | final fantasy | 2014-02
final fantasy xiv               | final fantasy | 2013-08
kingdom hearts hd 1 5           | kingdom hearts | 2014-03
kingdom hearts hd 2 5           | kingdom hearts | 2014-12
kingdom hearts ii               | kingdom hearts | 2006-03
kingdom hearts                  | kingdom hearts | 2002-09
tales of destiny 2              | tales of | 2002-11
tales of destiny directors cut  | tales of | 2006-11
tales of destiny                | tales of | 1997-12
tales of eternia                | tales of | 2000-12
tales of rebirth                | tales of | 2004-12
tales of legendia               | tales of | 2005-08
tales of the abyss              | tales of | 2006-10
tales of graces f               | tales of | 2012-03
tales of xillia 2               | tales of | 2014-08
tales of xillia                 | tales of | 2013-08
tales of symphonia chronicles   | tales of | 2014-02
tales of symphonia              | tales of | 2004-07
tales of zestiria               | tales of | 2015-10
tales of vesperia               | tales of | 2008-08
xenosaga episode iii            | xenosaga | 2006-08
xenosaga episode ii             | xenosaga | 2004-02
xenosaga episode i              | xenosaga | 2002-02
xenogears                       | xenogears | 1998-10
wild arms alter code f          | wild arms | 2005-07
wild arms 5                     | wild arms | 2006-06
wild arms 4                     | wild arms | 2005-03
wild arms 3                     | wild arms | 2002-04
shadow hearts from the new world | shadow hearts | 2005-10
shadow hearts covenant          | shadow hearts | 2004-02
shadow hearts                   | shadow hearts | 2001-07
suikoden                        | suikoden | 1996-12
grandia xtreme                  | grandia | 2002-08
grandia iii                     | grandia | 2007-02
grandia 3                       | grandia | 2007-02
grandia ii                      | grandia | 2000-12
grandia                         | grandia | 1999-06
ys v                            | ys | 2006-01
ni no kuni                      | ni no kuni | 2013-01
white knight chronicles ii      | white knight chronicles | 2011-02
white knight chronicles         | white knight chronicles | 2010-02
valkyrie profile 2              | valkyrie profile | 2006-09
valkyrie profile                | valkyrie profile | 2000-08
disgaea                         | disgaea | 2008-08
persona 4 arena ultimax         | persona | 2014-09
persona 4 arena                 | persona | 2012-08
persona 5                       | persona | 2017-04
drakengard 3                    | drakengard | 2014-05
drakengard 2                    | drakengard | 2006-03
drakengard                      | drakengard | 2004-03
# --- Capcom, Konami, Sega, Namco
resident evil outbreak file 2   | resident evil | 2005-02
resident evil outbreak          | resident evil | 2004-03
resident evil operation raccoon city | resident evil | 2012-03
resident evil revelations 2     | resident evil | 2015-02
resident evil 6                 | resident evil | 2012-10
resident evil 5                 | resident evil | 2009-03
resident evil 4                 | resident evil | 2005-01
resident evil 3                 | resident evil | 1999-11
resident evil 2                 | resident evil | 1998-01
resident evil                   | resident evil | 1996-03
dino crisis 2                   | dino crisis | 2000-09
dino crisis                     | dino crisis | 1999-07
onimusha dawn of dreams         | onimusha | 2006-03
onimusha 3                      | onimusha | 2004-02
onimusha 2                      | onimusha | 2002-03
onimusha                        | onimusha | 2001-01
devil may cry hd collection     | devil may cry | 2012-03
devil may cry 4                 | devil may cry | 2008-02
devil may cry 3                 | devil may cry | 2005-02
devil may cry 2                 | devil may cry | 2003-01
dmc devil may cry               | devil may cry | 2013-01
devil may cry                   | devil may cry | 2001-10
street fighter iv               | street fighter | 2009-02
super street fighter iv         | street fighter | 2010-04
ultra street fighter iv         | street fighter | 2014-06
viewtiful joe 2                 | viewtiful joe | 2004-11
viewtiful joe                   | viewtiful joe | 2003-06
mega man legends 2              | mega man legends | 2000-10
mega man legends                | mega man legends | 1998-12
parasite eve 2                  | parasite eve | 2000-09
parasite eve ii                 | parasite eve | 2000-09
parasite eve                    | parasite eve | 1998-09
silent hill 4 the room          | silent hill | 2004-09
silent hill 3                   | silent hill | 2003-05
silent hill 2                   | silent hill | 2001-09
silent hill origins             | silent hill | 2007-11
silent hill homecoming          | silent hill | 2008-09
silent hill shattered memories  | silent hill | 2009-12
silent hill hd collection       | silent hill | 2012-03
silent hill downpour            | silent hill | 2012-03
silent hill                     | silent hill | 1999-01
castlevania symphony of the night | castlevania | 1997-10
castlevania lament of innocence | castlevania | 2003-11
castlevania curse of darkness   | castlevania | 2005-11
castlevania lords of shadow 2   | castlevania | 2014-02
castlevania lords of shadow     | castlevania | 2010-10
metal gear solid 4              | metal gear solid | 2008-06
metal gear solid hd collection  | metal gear solid | 2011-11
metal gear solid peace walker   | metal gear solid | 2011-11
metal gear rising               | metal gear solid | 2013-02
metal gear solid ground zeroes  | metal gear solid | 2014-03
metal gear solid v the phantom pain | metal gear solid | 2015-09
metal gear solid 3              | metal gear solid | 2004-11
metal gear solid 2              | metal gear solid | 2001-11
metal gear solid                | metal gear solid | 1998-10
yakuza dead souls               | yakuza | 2012-03
yakuza 3                        | yakuza | 2010-02
yakuza 4                        | yakuza | 2011-03
yakuza 5                        | yakuza | 2015-12
yakuza                          | yakuza | 2006-09
sonic unleashed                 | sonic | 2008-11
sonic generations               | sonic | 2011-11
sonic ultimate genesis collection | sonic | 2009-02
sega ages 2500                  | sega ages 2500 | 2006-01
tekken 6                        | tekken | 2009-10
tekken hybrid                   | tekken | 2011-11
tekken tag tournament 2         | tekken | 2012-09
tekken 5                        | tekken | 2005-06
# --- Western
assassins creed iv black flag   | assassins creed | 2013-10
assassins creed iii             | assassins creed | 2012-10
assassins creed revelations     | assassins creed | 2011-11
assassins creed brotherhood     | assassins creed | 2010-11
assassins creed ii              | assassins creed | 2009-11
assassins creed rogue           | assassins creed | 2014-11
assassins creed                 | assassins creed | 2007-11
batman arkham asylum            | batman arkham | 2009-08
batman arkham city              | batman arkham | 2011-10
batman arkham origins           | batman arkham | 2013-10
bioshock infinite               | bioshock | 2013-03
bioshock 2                      | bioshock | 2010-02
bioshock                        | bioshock | 2008-10
call of duty 4                  | call of duty | 2007-11
call of duty world at war       | call of duty | 2008-11
call of duty modern warfare 2   | call of duty | 2009-11
call of duty modern warfare 3   | call of duty | 2011-11
call of duty modern warfare     | call of duty | 2007-11
call of duty black ops iii      | call of duty | 2015-11
call of duty black ops ii       | call of duty | 2012-11
call of duty black ops          | call of duty | 2010-11
call of duty ghosts             | call of duty | 2013-11
call of duty advanced warfare   | call of duty | 2014-11
call of duty 3                  | call of duty | 2006-11
darksiders ii                   | darksiders | 2012-08
darksiders 2                    | darksiders | 2012-08
darksiders                      | darksiders | 2010-01
demons souls                    | demons souls | 2010-02
dark souls ii                   | dark souls | 2014-03
dark souls 2                    | dark souls | 2014-03
dark souls                      | dark souls | 2011-10
dead space 3                    | dead space | 2013-02
dead space 2                    | dead space | 2011-01
dead space                      | dead space | 2008-10
dragon age ii                   | dragon age | 2011-03
dragon age 2                    | dragon age | 2011-03
dragon age inquisition          | dragon age | 2014-11
dragon age origins              | dragon age | 2009-11
fallout new vegas               | fallout | 2010-10
fallout 3                       | fallout | 2008-10
elder scrolls v skyrim          | elder scrolls | 2011-11
elder scrolls iv oblivion       | elder scrolls | 2007-03
grand theft auto iv             | grand theft auto | 2008-04
grand theft auto v              | grand theft auto | 2013-09
grand theft auto episodes from liberty city | grand theft auto | 2010-10
gran turismo 5 prologue         | gran turismo | 2008-04
gran turismo 5                  | gran turismo | 2010-11
gran turismo 6                  | gran turismo | 2013-12
mass effect 3                   | mass effect | 2012-03
mass effect 2                   | mass effect | 2011-01
mortal kombat vs dc universe    | mortal kombat | 2008-11
mortal kombat shaolin monks     | mortal kombat | 2005-10
mortal kombat 9                 | mortal kombat | 2011-04
mortal kombat komplete edition  | mortal kombat | 2011-04
mortal kombat                   | mortal kombat | 2011-04
need for speed shift 2          | need for speed | 2011-03
need for speed shift            | need for speed | 2009-09
need for speed hot pursuit      | need for speed | 2010-11
need for speed the run          | need for speed | 2011-11
need for speed most wanted      | need for speed | 2012-10
need for speed rivals           | need for speed | 2013-11
red dead redemption             | red dead | 2010-05
saints row the third            | saints row | 2011-11
saints row iv                   | saints row | 2013-08
saints row 2                    | saints row | 2009-02
borderlands 2                   | borderlands | 2012-09
borderlands the pre sequel      | borderlands | 2014-10
borderlands                     | borderlands | 2009-10
guitar hero iii                 | guitar hero | 2007-10
guitar hero world tour          | guitar hero | 2008-10
guitar hero metallica           | guitar hero | 2009-03
guitar hero smash hits          | guitar hero | 2009-06
guitar hero 5                   | guitar hero | 2009-09
guitar hero warriors of rock    | guitar hero | 2010-09
guitar hero aerosmith           | guitar hero | 2008-06
prince of persia the forgotten sands | prince of persia | 2010-05
prince of persia warrior within | prince of persia | 2004-12
prince of persia the two thrones | prince of persia | 2005-12
prince of persia the sands of time | prince of persia | 2003-11
prince of persia trilogy        | prince of persia | 2010-12
prince of persia classic trilogy | prince of persia | 2010-12
prince of persia                | prince of persia | 2008-12
skate 3                         | skate | 2010-05
skate 2                         | skate | 2009-01
skate                           | skate | 2007-09
zone of the enders hd collection | zone of the enders | 2012-10
zone of the enders              | zone of the enders | 2001-03
)RULES";
