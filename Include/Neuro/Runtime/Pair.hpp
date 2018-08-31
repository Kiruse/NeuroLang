////////////////////////////////////////////////////////////////////////////////
// Like STL pair, used as a preliminary substitute for hash maps until I'm in
// the mood to actually implement them.
// -----
// Copyright (c) Kiruse 2018
// License: GPL 3.0
#pragma once

namespace Neuro
{
    template<typename FirstT, typename SecondT>
    struct Pair {
        FirstT first;
        SecondT second;
        
        Pair(const FirstT& first, const SecondT& second) : first(first), second(second) {}
        Pair(FirstT&& first, SecondT&& second) : first(first), second(second) {}
    };
}
