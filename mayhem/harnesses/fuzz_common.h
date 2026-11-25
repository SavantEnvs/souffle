// Shared helpers for the souffle fuzz harnesses (mayhem/ layer, not upstream code).
//
// The in-process harnesses parse an in-memory program with a fresh souffle::Global per input whose
// options come from the real CLI parser (MainConfig::processArgs + getMainOptions), as if `souffle
// --no-preprocessor -j1 -F<tmp> -D<tmp> fuzz.dl` had been run. The semantic checks, AST/RAM
// transforms and the interpreter are fuzzed by the file-input target fuzz_cli (the real souffle CLI),
// because every ast::transform::Transformer::apply exit()s on semantic errors.
#pragma once

#include "Global.h"
#include "MainDriver.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/typesystem/Type.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "parser/ParserDriver.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fuzzsouffle {

// A per-process scratch directory under $TMPDIR (fallback /tmp), used as fact-dir and output-dir.
inline const std::string& scratchDir() {
    static std::string dir = [] {
        const char* t = std::getenv("TMPDIR");
        std::string tmpl = std::string(t && *t ? t : "/tmp") + "/souffle-fuzz-XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        if (mkdtemp(buf.data()) == nullptr) abort();
        return std::string(buf.data());
    }();
    return dir;
}

// Options as the souffle CLI would set them (defaults included).
inline void configure(souffle::Global& glb) {
    std::string fdir = "--fact-dir=" + scratchDir();
    std::string odir = "--output-dir=" + scratchDir();
    std::vector<std::string> args = {"souffle", "--no-preprocessor", "--jobs=1", "--no-warn", fdir, odir,
            "fuzz.dl"};
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);
    glb.config().processArgs(static_cast<int>(args.size()), argv.data(), "", "", souffle::getMainOptions());
    glb.config().set("version", souffle::packageVersion());
}

// Parse. The translation unit keeps references to the reports, so they live alongside it.
struct Parsed {
    souffle::ErrorReport errReport;
    souffle::DebugReport debugReport;
    souffle::Own<souffle::ast::TranslationUnit> tu;
};
// Returns false on parse errors (souffle::main would exitIfErrors() here).
inline bool parse(souffle::Global& glb, const std::string& code, Parsed& p) {
    p.tu = souffle::ParserDriver::parseTranslationUnit(glb, "fuzz.dl", code, false, p.errReport, p.debugReport);
    return p.tu && p.tu->getErrorReport().getNumErrors() == 0;
}

// Swallow std::cout/std::cerr so the per-input reports and program output don't flood the fuzzer log.
struct QuietStreams {
    std::ostringstream sink;
    std::streambuf* oldOut;
    std::streambuf* oldErr;
    QuietStreams() : oldOut(std::cout.rdbuf(sink.rdbuf())), oldErr(std::cerr.rdbuf(sink.rdbuf())) {}
    ~QuietStreams() {
        std::cout.rdbuf(oldOut);
        std::cerr.rdbuf(oldErr);
    }
};

}  // namespace fuzzsouffle
