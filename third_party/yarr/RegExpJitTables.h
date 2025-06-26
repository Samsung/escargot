std::unique_ptr<CharacterClass> wordcharCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_ranges.append(CharacterRange(0x30, 0x39));
    characterClass->m_ranges.append(CharacterRange(0x41, 0x5a));
    characterClass->m_matches.append(0x5f);
    characterClass->m_ranges.append(CharacterRange(0x61, 0x7a));
    characterClass->m_characterWidths = CharacterClassWidths::HasBMPChars;
    return characterClass;
}

std::unique_ptr<CharacterClass> wordUnicodeIgnoreCaseCharCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_ranges.append(CharacterRange(0x30, 0x39));
    characterClass->m_ranges.append(CharacterRange(0x41, 0x5a));
    characterClass->m_matches.append(0x5f);
    characterClass->m_ranges.append(CharacterRange(0x61, 0x7a));
    characterClass->m_matchesUnicode.append(0x017f);
    characterClass->m_matchesUnicode.append(0x212a);
    characterClass->m_characterWidths = CharacterClassWidths::HasBMPChars;
    return characterClass;
}

std::unique_ptr<CharacterClass> nonwordcharCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_ranges.append(CharacterRange(0x00, 0x2f));
    characterClass->m_ranges.append(CharacterRange(0x3a, 0x40));
    characterClass->m_ranges.append(CharacterRange(0x5b, 0x5e));
    characterClass->m_matches.append(0x60);
    characterClass->m_ranges.append(CharacterRange(0x7b, 0x7f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x0080, 0x10ffff));
    characterClass->m_characterWidths = CharacterClassWidths::HasBothBMPAndNonBMP;
    return characterClass;
}

std::unique_ptr<CharacterClass> nonwordUnicodeIgnoreCaseCharCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_ranges.append(CharacterRange(0x00, 0x2f));
    characterClass->m_ranges.append(CharacterRange(0x3a, 0x40));
    characterClass->m_ranges.append(CharacterRange(0x5b, 0x5e));
    characterClass->m_matches.append(0x60);
    characterClass->m_ranges.append(CharacterRange(0x7b, 0x7f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x0080, 0x017e));
    characterClass->m_rangesUnicode.append(CharacterRange(0x0180, 0x2129));
    characterClass->m_rangesUnicode.append(CharacterRange(0x212b, 0x10ffff));
    characterClass->m_characterWidths = CharacterClassWidths::HasBothBMPAndNonBMP;
    return characterClass;
}

std::unique_ptr<CharacterClass> newlineCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_matches.append(0x0a);
    characterClass->m_matches.append(0x0d);
    characterClass->m_rangesUnicode.append(CharacterRange(0x2028, 0x2029));
    characterClass->m_characterWidths = CharacterClassWidths::HasBMPChars;
    return characterClass;
}

std::unique_ptr<CharacterClass> spacesCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_ranges.append(CharacterRange(0x09, 0x0d));
    characterClass->m_matches.append(0x20);
    characterClass->m_matchesUnicode.append(0x00a0);
    characterClass->m_matchesUnicode.append(0x1680);
    characterClass->m_rangesUnicode.append(CharacterRange(0x2000, 0x200a));
    characterClass->m_rangesUnicode.append(CharacterRange(0x2028, 0x2029));
    characterClass->m_matchesUnicode.append(0x202f);
    characterClass->m_matchesUnicode.append(0x205f);
    characterClass->m_matchesUnicode.append(0x3000);
    characterClass->m_matchesUnicode.append(0xfeff);
    characterClass->m_characterWidths = CharacterClassWidths::HasBMPChars;
    return characterClass;
}

std::unique_ptr<CharacterClass> nonspacesCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_ranges.append(CharacterRange(0x00, 0x08));
    characterClass->m_ranges.append(CharacterRange(0x0e, 0x1f));
    characterClass->m_ranges.append(CharacterRange(0x21, 0x7f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x0080, 0x009f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x00a1, 0x167f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x1681, 0x1fff));
    characterClass->m_rangesUnicode.append(CharacterRange(0x200b, 0x2027));
    characterClass->m_rangesUnicode.append(CharacterRange(0x202a, 0x202e));
    characterClass->m_rangesUnicode.append(CharacterRange(0x2030, 0x205e));
    characterClass->m_rangesUnicode.append(CharacterRange(0x2060, 0x2fff));
    characterClass->m_rangesUnicode.append(CharacterRange(0x3001, 0xfefe));
    characterClass->m_rangesUnicode.append(CharacterRange(0xff00, 0x10ffff));
    characterClass->m_characterWidths = CharacterClassWidths::HasBothBMPAndNonBMP;
    return characterClass;
}

std::unique_ptr<CharacterClass> digitsCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_ranges.append(CharacterRange(0x30, 0x39));
    characterClass->m_characterWidths = CharacterClassWidths::HasBMPChars;
    return characterClass;
}

std::unique_ptr<CharacterClass> nondigitsCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_ranges.append(CharacterRange(0x00, 0x2f));
    characterClass->m_ranges.append(CharacterRange(0x3a, 0x7f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x0080, 0x10ffff));
    characterClass->m_characterWidths = CharacterClassWidths::HasBothBMPAndNonBMP;
    return characterClass;
}

