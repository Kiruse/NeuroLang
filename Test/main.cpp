////////////////////////////////////////////////////////////////////////////////
// Neuro unit testing program.
// -----
// Copyright (c) Kiruse 2018 Germany
#include "NeuroTestingFramework.hpp"
#include "UnitTests.hpp"

int main(int argc, char* argv[]) {
    TestNeuroBuffer();
    TestNeuroString();
    TestNeuroSet();
    TestNeuroMaybe();
    TestNeuroMap();
}
