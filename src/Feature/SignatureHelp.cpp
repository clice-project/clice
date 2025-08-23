#include "Compiler/Compilation.h"
#include "Feature/SignatureHelp.h"
#include "clang/Sema/CodeCompleteConsumer.h"

namespace clice::feature {

namespace {

// Returns the index of the parameter matching argument number "Arg.
// This is usually just "Arg", except for variadic functions/templates, where
// "Arg" might be higher than the number of parameters. When that happens, we
// assume the last parameter is variadic and assume all further args are
// part of it.
int param_index(const clang::CodeCompleteConsumer::OverloadCandidate& candidate, int arg) {
    int params_count = candidate.getNumParams();
    if(auto* T = candidate.getFunctionType()) {
        if(auto* proto = T->getAs<clang::FunctionProtoType>()) {
            if(proto->isVariadic()) {
                params_count += 1;
            }
        }
    }
    return std::min(arg, std::max(params_count - 1, 0));
}

class Builder {
public:
    Builder(proto::SignatureHelp& result) : result(result) {
        // std::vector<int>(1, 2,)
    }

private:
    proto::SignatureHelp& result;
};

class Collector final : public clang::CodeCompleteConsumer {
public:
    Collector(clang::CodeCompleteOptions options) :
        clang::CodeCompleteConsumer(options),
        info(std::make_shared<clang::GlobalCodeCompletionAllocator>()) {}

    void ProcessOverloadCandidates(clang::Sema& sema,
                                   std::uint32_t current_arg,
                                   OverloadCandidate* candidates,
                                   std::uint32_t candidate_count,
                                   clang::SourceLocation open_paren_loc,
                                   bool braced) final {
        proto::SignatureHelp help;
        help.signatures.reserve(candidate_count);

        // FIXME: How can we determine the "active overload candidate"?
        // Right now the overloaded candidates seem to be provided in a "best fit"
        // order, so I'm not too worried about this.
        help.activeSignature = 0;

        auto range = llvm::make_range(candidates, candidates + candidate_count);
        for(auto& candidate: range) {
            /// We want to avoid showing instantiated signatures, because they may be
            /// long in some cases (e.g. when 'T' is substituted with 'std::string', we
            /// would get 'std::basic_string<char>').
            /// FIXME: In fact, in such case, we may resugar the template arguments.
            if(auto func = candidate.getFunction()) {
                if(auto pattern = func->getTemplateInstantiationPattern()) {
                    candidate = OverloadCandidate(pattern);
                }
            }
        }
    }

    clang::CodeCompletionAllocator& getAllocator() final {
        return info.getAllocator();
    }

    clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() final {
        return info;
    }

private:
    clang::CodeCompletionTUInfo info;
};

}  // namespace

proto::SignatureHelp signature_help(CompilationParams& params,
                                    const config::SignatureHelpOption& option) {
    proto::SignatureHelp help;
    auto consumer = new Collector({});
    if(auto info = complete(params, consumer)) {}
    return help;
}

}  // namespace clice::feature

