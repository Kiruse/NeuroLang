////////////////////////////////////////////////////////////////////////////////
// Possibly the most complex "unit test" we have, although ironically it's not
// a unit test. It's more of a stress test, because eventually the GC will
// involve a more or less complex AI. And as with everything complex like that,
// it would become increasingly difficult to write a suitable unit test for it.
// Accordingly, I deemed a stress test more useful as it throws thousands of
// scenarios at the garbage collector and verifies that, even after a prolongued
// period, everything still works.
// -----
// Copyright (c) Kiruse 2018 Germany
// License: GPL 3.0
#include "GC/NeuroGC.hpp"

using namespace Neuro;
using namespace Neuro::Runtime;

int main(int argc, char** argv)
{
    
}
