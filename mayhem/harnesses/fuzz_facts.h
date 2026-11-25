// Shared body of the fact-file reader targets (fuzz_facts_csv, fuzz_facts_json; mayhem/ layer).
//
// The input bytes are the contents of a `.input` fact file. The reader options are the ones souffle's
// own compiler produces: a fixed program declaring one relation over every attribute kind the reader
// decodes (number, unsigned, float, symbol, records, and for CSV also ADT and enum) is parsed once and run through IODefaultsTransformer
// + IOAttributesTransformer, which fill in IO/name/fact-dir/types/params exactly as for
// `souffle --fact-dir=<tmp> prog.dl`. Per input, the harness writes the bytes to the fact file and does
// what interpreter::Engine does for an `.input` (src/interpreter/Engine.cpp, CASE(IO)):
// IOSystem::getReader(directive, symbols, records)->readAll(relation). The interpreter turns a reader
// exception into exit(); here the documented exception is caught instead.
//
// State: the directive map and the IOSystem singleton are read-only after initialisation; the symbol
// table, record table and relation sink are fresh per input; the fact file is rewritten per input.
#pragma once

#include "fuzz_common.h"
#include "ast/Directive.h"
#include "ast/transform/IOAttributes.h"
#include "ast/transform/IODefaults.h"
#include "souffle/datastructure/RecordTableImpl.h"
#include "souffle/datastructure/SymbolTableImpl.h"
#include "souffle/io/IOSystem.h"

#include <fstream>
#include <map>

namespace fuzzsouffle {

// Same table types as interpreter::Engine (src/interpreter/Engine.h).
using EngineRecordTable = souffle::SpecializedRecordTable<0, 1, 2, 3, 4, 5, 6, 7, 8, 9>;

// A relation sink: ReadStream::readAll(T&) only needs insert(const RamDomain*).
struct TupleSink {
    std::size_t arity;
    std::vector<souffle::RamDomain> data;
    void insert(const souffle::RamDomain* t) { data.insert(data.end(), t, t + arity); }
};

// Types and relation shared by the targets. Every attribute kind the readers decode.
inline const char* kTypes = R"(
.type Pair = [a: number, b: symbol]
.type Nest = [x: Pair, y: float]
.type Shape = Circle {r: float} | Rect {w: unsigned, h: unsigned} | Tagged {p: Pair, s: symbol} | Nothing {}
.type Color = Red {} | Green {} | Blue {}
)";

// `decl` declares relation r; `input` is its .input directive. Returns the directive's final options.
inline std::map<std::string, std::string> inputDirective(const std::string& decl, const std::string& input) {
    souffle::Global glb;
    configure(glb);
    const std::string program = std::string(kTypes) + decl + "\n" + input + "\n";
    Parsed p;
    if (!parse(glb, program, p)) abort();
    souffle::ast::transform::IODefaultsTransformer().apply(*p.tu);
    souffle::ast::transform::IOAttributesTransformer().apply(*p.tu);
    for (const auto* d : p.tu->getProgram().getDirectives()) {
        if (d->getType() == souffle::ast::DirectiveType::input) return d->getParameters();
    }
    abort();
}

inline int readFacts(const std::map<std::string, std::string>& directive, std::size_t arity,
        const std::string& fileName, const uint8_t* data, size_t size) {
    QuietStreams quiet;
    {
        std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!out) abort();
    }
    souffle::SymbolTableImpl symbols;
    EngineRecordTable records;
    TupleSink rel{arity, {}};
    try {
        souffle::IOSystem::getInstance().getReader(directive, symbols, records)->readAll(rel);
    } catch (std::exception& e) {
        // Rejected fact file (souffle reports "Error loading r data: ..." and exits).
    }
    return 0;
}

}  // namespace fuzzsouffle
