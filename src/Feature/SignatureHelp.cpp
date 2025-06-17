#include "Compiler/Compilation.h"
#include "Feature/SignatureHelp.h"
#include "clang/Sema/CodeCompleteConsumer.h"

namespace clice::feature {

namespace {

class SignatureHelpCollector final : public clang::CodeCompleteConsumer {
public:
    SignatureHelpCollector(clang::CodeCompleteOptions options) :
        clang::CodeCompleteConsumer(options), allocator(new clang::GlobalCodeCompletionAllocator()),
        info(allocator) {}

    void ProcessOverloadCandidates(clang::Sema& sema,
                                   unsigned CurrentArg,
                                   OverloadCandidate* candidates,
                                   unsigned count,
                                   clang::SourceLocation openParLoc,
                                   bool braced) final {
        llvm::outs() << "ProcessOverloadCandidates\n";
        auto range = llvm::make_range(candidates, candidates + count);
        for(auto& candidate: range) {
            switch(candidate.getKind()) {
                case clang::CodeCompleteConsumer::OverloadCandidate::CK_Function: {
                    candidate.getFunction()->dump();
                    break;
                }
                case clang::CodeCompleteConsumer::OverloadCandidate::CK_FunctionTemplate: {
                    candidate.getFunctionTemplate()->dump();
                    break;
                }
                case clang::CodeCompleteConsumer::OverloadCandidate::CK_FunctionType: {
                    candidate.getFunctionType()->dump();
                    break;
                }
                case clang::CodeCompleteConsumer::OverloadCandidate::CK_FunctionProtoTypeLoc: {
                    candidate.getFunctionProtoTypeLoc().dump();
                    break;
                }
                case clang::CodeCompleteConsumer::OverloadCandidate::CK_Template: {
                    candidate.getTemplate()->dump();
                    break;
                }
                case clang::CodeCompleteConsumer::OverloadCandidate::CK_Aggregate: {
                    candidate.getAggregate()->dump();
                    break;
                }
            }
        }
    }

    clang::CodeCompletionAllocator& getAllocator() final {
        return *allocator;
    }

    clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() final {
        return info;
    }

private:
    std::shared_ptr<clang::GlobalCodeCompletionAllocator> allocator;
    clang::CodeCompletionTUInfo info;
};

}  // namespace

std::vector<SignatureHelpItem> signatureHelp(CompilationParams& params,
                                             const config::SignatureHelpOption& option) {
    std::vector<SignatureHelpItem> items;
    auto consumer = new SignatureHelpCollector({});
    if(auto info = complete(params, consumer)) {}
    return items;
}

}  // namespace clice::feature

