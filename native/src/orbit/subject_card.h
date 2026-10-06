// 0.15 stage 4: the new UI's subject card, made from the SAME text as the old picture card (orbit/card_text.h): the name, one line of kind, ONE big key number with a short caption,
// four to six key values, and every line of the old card for the data sheet ("All values"). Nothing is re-worded and nothing is worked out again: every piece is a piece of one of
// the old card's lines, word for word, so the two can never disagree (tested against every card of the real system).
#pragma once

#include <string>
#include <vector>

#include "card_text.h"

namespace orbit {

struct SubjectValue { std::string label, value; };       // label empty: a plain line (the line is `value`); else the line is label + ": " + value

struct SubjectCard {
    std::string name;                                    // "Earth"
    std::string kind;                                    // "rocky planet (home)"
    std::string keyValue, keyCaption;                    // "1.00 AU", "from the Sun"
    std::vector<SubjectValue> values;                    // the key values, 4 to 6 (fewer only when the card has no more lines)
    std::vector<std::string> all;                        // the old card's lines, exactly (the data sheet)
};

// beltKey: for a belt, SolarSystem::belts[i].key ("main", "trojan", "kuiper", "oort"); else "".
SubjectCard subjectCard(const Selection& sel, const std::string& beltKey, const Card& card);

}  // namespace orbit
