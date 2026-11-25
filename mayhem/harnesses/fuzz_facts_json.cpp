// Fuzz target: the JSON fact reader (ReadFileJSON / ReadStreamJSON) on the input bytes as a `.json`
// file (`.input r(IO=jsonfile)`); tuples as lists or as objects keyed by attribute name. ReadStreamJSON
// has no ADT support (it throws on a `+` type), so the relation uses numbers, symbols and records only.
#include "fuzz_facts.h"

static const char* kDecl = ".decl r(i: number, u: unsigned, f: float, s: symbol, p: Pair, n: Nest)";

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const auto directive = fuzzsouffle::inputDirective(kDecl, ".input r(IO=jsonfile)");
    return fuzzsouffle::readFacts(directive, 6, fuzzsouffle::scratchDir() + "/r.json", data, size);
}
