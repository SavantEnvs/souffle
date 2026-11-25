// Fuzz target: the Datalog front end — flex scanner, bison parser, ParserDriver/AST construction and
// the AST printer — on the input bytes as a .dl program (`souffle --no-preprocessor --show
// initial-datalog fuzz.dl`).
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fuzzsouffle::QuietStreams quiet;
    souffle::Global glb;
    fuzzsouffle::configure(glb);
    std::string code(reinterpret_cast<const char*>(data), size);
    fuzzsouffle::Parsed p;
    if (!fuzzsouffle::parse(glb, code, p)) return 0;
    quiet.sink << p.tu->getProgram() << std::endl;
    return 0;
}
