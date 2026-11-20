/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * FontDaw
 * The notation-first DAW
 *
 * Copyright (C) 2026 FontDaw contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "commandsearch.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace muse;
using namespace mu::pointertools;

namespace {
constexpr int RANK_NONE = -1;

bool isWordChar(char16_t ch)
{
    return Char::isLetter(ch) || Char::isDigit(ch);
}

//! The text split on white space
std::vector<std::u16string> splitWords(const std::u16string& text)
{
    std::vector<std::u16string> words;
    std::u16string current;
    for (char16_t ch : text) {
        if (Char::isSpace(ch)) {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        words.push_back(current);
    }
    return words;
}

//! 0: text starts with the word, 1: a word of text starts with it, 2: text contains it, else RANK_NONE
//! (the caller adds 3 for descriptions)
int rankIn(const std::u16string& text, const std::u16string& word)
{
    if (text.compare(0, word.size(), word) == 0 && text.size() >= word.size()) {
        return 0;
    }

    bool contains = false;
    size_t pos = text.find(word);
    while (pos != std::u16string::npos) {
        contains = true;
        if (pos == 0 || !isWordChar(text[pos - 1])) {
            return 1;
        }
        pos = text.find(word, pos + 1);
    }
    return contains ? 2 : RANK_NONE;
}

int rankOfWord(const std::u16string& title, const std::u16string& description, const std::u16string& word)
{
    const int inTitle = rankIn(title, word);
    if (inTitle != RANK_NONE) {
        return inTitle;
    }

    // In the description, only "a word starts with" and "contains" are told apart: 3 and 4
    const int inDescription = rankIn(description, word);
    if (inDescription == RANK_NONE) {
        return RANK_NONE;
    }
    return inDescription <= 1 ? 3 : 4;
}
}

SearchHitList mu::pointertools::rankEntries(const CommandEntryList& entries, const String& text)
{
    const std::vector<std::u16string> words = splitWords(text.toLower().trimmed().toStdU16String());

    SearchHitList hits;
    hits.reserve(entries.size());

    if (words.empty()) {
        for (const CommandEntry& entry : entries) {
            hits.push_back({ &entry, 0 });
        }
        return hits;
    }

    for (const CommandEntry& entry : entries) {
        const std::u16string title = entry.title.toLower().toStdU16String();
        const std::u16string description = entry.description.toLower().toStdU16String();

        int worst = 0;
        bool matches = true;
        for (const std::u16string& word : words) {
            const int rank = rankOfWord(title, description, word);
            if (rank == RANK_NONE) {
                matches = false;
                break;
            }
            worst = std::max(worst, rank);
        }

        if (matches) {
            hits.push_back({ &entry, worst });
        }
    }

    // The input order is the last tie-breaker, kept by the stable sort
    std::stable_sort(hits.begin(), hits.end(), [](const SearchHit& a, const SearchHit& b) {
        if (a.matchRank != b.matchRank) {
            return a.matchRank < b.matchRank;
        }
        if (a.entry->section != b.entry->section) {
            return static_cast<int>(a.entry->section) < static_cast<int>(b.entry->section);
        }
        return a.entry->title.size() < b.entry->title.size();
    });

    return hits;
}
