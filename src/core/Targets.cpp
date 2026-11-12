// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#include "Targets.h"

namespace w2a {

const std::vector<TargetInfo>& allTargets() {
    static const std::vector<TargetInfo> list = {
        {Target::Svx8,  "8svx",  "8svx", 4},
        {Target::Aiff,  "aiff",  "aiff", 8},
        {Target::Svx16, "16sv",  "16sv", 4},
        {Target::Maud,  "maud",  "maud", 2},
    };
    return list;
}

const TargetInfo& targetInfo(Target t) {
    for (const auto& i : allTargets())
        if (i.id == t) return i;
    return allTargets().front();
}

Target targetFromKey(const std::string& key, Target fallback) {
    for (const auto& i : allTargets())
        if (key == i.key) return i.id;
    return fallback;
}

} // namespace w2a
