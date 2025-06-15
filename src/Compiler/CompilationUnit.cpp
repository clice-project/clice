#include "CompilationUnitImpl.h"
#include "Index/USR.h"
#include "AST/Utility.h"

namespace clice {

CompilationUnit::~CompilationUnit() {
    if(impl && impl->action) {
        impl->action->EndSourceFile();
    }

    delete impl;
}

const llvm::DenseSet<clang::FileID>& CompilationUnit::files() {
    if(impl->allFiles.empty()) {
        /// FIXME: handle preamble and embed file id.
        for(auto& [fid, diretive]: directives()) {
            for(auto& include: diretive.includes) {
                if(!include.skipped) {
                    impl->allFiles.insert(include.fid);
                }
            }
        }
        impl->allFiles.insert(impl->SM.getMainFileID());
    }
    return impl->allFiles;
}

clang::FileID CompilationUnit::file_id(llvm::StringRef file) {
    auto entry = impl->SM.getFileManager().getFileRef(file);
    if(entry) {
        return impl->SM.translateFile(*entry);
    }

    return clang::FileID();
}

clang::FileID CompilationUnit::file_id(clang::SourceLocation location) {
    return impl->SM.getFileID(location);
}

std::uint32_t CompilationUnit::file_offset(clang::SourceLocation location) {
    return impl->SM.getFileOffset(location);
}

clang::SourceLocation CompilationUnit::start_location(clang::FileID fid) {
    return impl->SM.getLocForStartOfFile(fid);
}

clang::SourceLocation CompilationUnit::end_location(clang::FileID fid) {
    return impl->SM.getLocForEndOfFile(fid);
}

clang::SourceLocation CompilationUnit::spelling_location(clang::SourceLocation loc) {
    return impl->SM.getSpellingLoc(loc);
}

clang::SourceLocation CompilationUnit::expansion_location(clang::SourceLocation location) {
    return impl->SM.getExpansionLoc(location);
}

auto CompilationUnit::decompose_location(clang::SourceLocation location)
    -> std::pair<clang::FileID, std::uint32_t> {
    return impl->SM.getDecomposedLoc(location);
}

clang::SourceLocation CompilationUnit::include_location(clang::FileID fid) {
    return impl->SM.getIncludeLoc(fid);
}

clang::PresumedLoc CompilationUnit::presumed_location(clang::SourceLocation location) {
    return impl->SM.getPresumedLoc(location, false);
}

llvm::ArrayRef<clang::syntax::Token> CompilationUnit::spelled_tokens(clang::FileID fid) {
    return impl->buffer->spelledTokens(fid);
}

llvm::ArrayRef<clang::syntax::Token> CompilationUnit::expanded_tokens(clang::SourceRange range) {
    return impl->buffer->expandedTokens(range);
}

llvm::StringRef CompilationUnit::token_spelling(clang::SourceLocation location) {
    return getTokenSpelling(impl->SM, location);
}

llvm::StringRef CompilationUnit::module_name() {
    return impl->instance->getPreprocessor().getNamedModuleName();
}

bool CompilationUnit::is_module_interface_unit() {
    return impl->instance->getPreprocessor().isInNamedInterfaceUnit();
}

clang::LangOptions& CompilationUnit::lang_options() {
    return impl->instance->getLangOpts();
}

std::vector<std::string> CompilationUnit::deps() {
    llvm::StringSet<> deps;

    /// FIXME: consider `#embed` and `__has_embed`.

    for(auto& [fid, diretive]: directives()) {
        for(auto& include: diretive.includes) {
            if(!include.skipped) {
                deps.try_emplace(file_path(include.fid));
            }
        }

        for(auto& hasInclude: diretive.hasIncludes) {
            if(hasInclude.fid.isValid()) {
                deps.try_emplace(file_path(hasInclude.fid));
            }
        }
    }

    std::vector<std::string> result;

    for(auto& deps: deps) {
        result.emplace_back(deps.getKey().str());
    }

    return result;
}

llvm::StringRef CompilationUnit::file_path(clang::FileID fid) {
    assert(fid.isValid() && "Invalid fid");
    if(auto it = impl->pathCache.find(fid); it != impl->pathCache.end()) {
        return it->second;
    }

    auto entry = impl->SM.getFileEntryRefForID(fid);
    assert(entry && "Invalid file entry");

    llvm::SmallString<128> path;

    /// Try to get the real path of the file.
    auto name = entry->getName();
    if(auto error = llvm::sys::fs::real_path(name, path)) {
        /// If failed, use the virtual path.
        path = name;
    }
    assert(!path.empty() && "Invalid file path");

    /// Allocate the path in the storage.
    auto size = path.size();
    auto data = impl->pathStorage.Allocate<char>(size + 1);
    memcpy(data, path.data(), size);
    data[size] = '\0';

    auto [it, inserted] = impl->pathCache.try_emplace(fid, llvm::StringRef(data, size));
    assert(inserted && "File path already exists");
    return it->second;
}

llvm::StringRef CompilationUnit::file_content(clang::FileID fid) {
    return impl->SM.getBufferData(fid);
}

std::pair<clang::FileID, LocalSourceRange>
    CompilationUnit::decompose_range(clang::SourceRange range) {
    auto [begin, end] = range;
    assert(begin.isValid() && end.isValid() && "Invalid source range");
    assert(begin.isFileID() && end.isValid() && "Input source range should be a file range");

    if(begin == end) {
        auto [fid, offset] = decompose_location(begin);
        return {
            fid,
            {offset, offset + getTokenLength(impl->SM, end)}
        };
    } else {
        auto [beginFID, beginOffset] = decompose_location(begin);
        auto [endFID, endOffset] = decompose_location(end);
        if(beginFID == endFID) {
            return {
                beginFID,
                {beginOffset, endOffset + getTokenLength(impl->SM, end)}
            };
        } else {
            auto content = file_content(beginFID);
            return {
                beginFID,
                {beginOffset, static_cast<uint32_t>(content.size())}
            };
        }
    }
}

std::pair<clang::FileID, LocalSourceRange>
    CompilationUnit::decompose_expansion_range(clang::SourceRange range) {
    auto [begin, end] = range;
    if(begin == end) {
        return decompose_range(expansion_location(begin));
    } else {
        return decompose_range(
            clang::SourceRange(expansion_location(begin), expansion_location(end)));
    }
}

index::SymbolID CompilationUnit::getSymbolID(const clang::NamedDecl* decl) {
    uint64_t hash;
    auto iter = impl->symbolHashCache.find(decl);
    if(iter != impl->symbolHashCache.end()) {
        hash = iter->second;
    } else {
        llvm::SmallString<128> USR;
        index::generateUSRForDecl(decl, USR);
        hash = llvm::xxh3_64bits(USR);
        impl->symbolHashCache.try_emplace(decl, hash);
    }
    return index::SymbolID{hash, getDeclName(decl)};
}

index::SymbolID CompilationUnit::getSymbolID(const clang::MacroInfo* macro) {
    std::uint64_t hash;
    auto name = getTokenSpelling(impl->SM, macro->getDefinitionLoc());
    auto iter = impl->symbolHashCache.find(macro);
    if(iter != impl->symbolHashCache.end()) {
        hash = iter->second;
    } else {
        llvm::SmallString<128> USR;
        index::generateUSRForMacro(name, macro->getDefinitionLoc(), impl->SM, USR);
        hash = llvm::xxh3_64bits(USR);
        impl->symbolHashCache.try_emplace(macro, hash);
    }
    return index::SymbolID{hash, name.str()};
}

clang::FileID CompilationUnit::getInterestedFile() {
    return impl->interested;
}

llvm::StringRef CompilationUnit::getInterestedFileContent() {
    return file_content(impl->interested);
}

bool CompilationUnit::isBuiltinFile(clang::FileID fid) {
    auto path = file_path(fid);
    return path == "<built-in>" || path == "<command line>" || path == "<scratch space>";
}

clang::TranslationUnitDecl* CompilationUnit::tu() {
    return impl->instance->getASTContext().getTranslationUnitDecl();
}

llvm::DenseMap<clang::FileID, Directive>& CompilationUnit::directives() {
    return impl->m_directives;
}

TemplateResolver& CompilationUnit::resolver() {
    assert(impl->m_resolver && "Template resolver is not available");
    return *impl->m_resolver;
}

clang::Sema& CompilationUnit::sema() {
    return impl->instance->getSema();
}

clang::ASTContext& CompilationUnit::context() {
    return impl->instance->getASTContext();
}
}  // namespace clice
