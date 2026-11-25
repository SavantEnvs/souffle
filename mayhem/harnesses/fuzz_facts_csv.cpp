// Fuzz target: the CSV fact reader (ReadFileCSV / ReadStreamCSV) on the input bytes as a `.facts`
// file of a relation using every attribute kind. Two directive variants, as a program could declare
// them: the default tab-separated form and `rfc4180=true` (quoted fields, `,` delimiter).
// The first input byte picks the variant (bit 0); the rest is the file.
#include "fuzz_facts.h"

static const char* kDecl = ".decl r(i: number, u: unsigned, f: float, s: symbol, p: Pair, n: Nest, sh: Shape, c: Color)";

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const auto plain = fuzzsouffle::inputDirective(kDecl, ".input r(IO=file)");
    static const auto rfc = fuzzsouffle::inputDirective(kDecl, ".input r(IO=file, rfc4180=true)");
    if (size == 0) return -1;
    const auto& directive = (data[0] & 1) ? rfc : plain;
    return fuzzsouffle::readFacts(directive, 8, fuzzsouffle::scratchDir() + "/r.facts", data + 1, size - 1);
}
