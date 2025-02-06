#include "Basic/SourceConverter.h"
#include "Server/Server.h"

namespace clice {

async::Task<std::vector<proto::Location>>
    Server::lookup(const proto::TextDocumentPositionParams& params, RelationKind kind) {
    auto path = SourceConverter::toPath(params.textDocument.uri);
    llvm::StringMap<std::vector<LocalSourceRange>> results;

    /// TODO: For opened file, use their im memory context rather than reading from disk.
    auto content = co_await async::fs::read(path);
    if(!content) {
        co_return std::vector<proto::Location>();
    }

    auto offset = SC.toOffset(*content, params.position);
    co_await indexer.lookup(path,
                            offset,
                            [&](llvm::StringRef path, const index::SymbolIndex::Symbol& symbol) {
                                for(auto relation: symbol.relations()) {
                                    if(relation.kind() & kind) {
                                        results[path].emplace_back(*relation.range());
                                    }
                                }
                            });

    std::vector<proto::Location> locations;
    for(auto& [path, ranges]: results) {
        auto content = co_await async::fs::read(path.str());
        for(auto& range: ranges) {
            locations.emplace_back(proto::Location{
                .uri = SourceConverter::toURI(path),
                .range = SC.toRange(range, *content),
            });
        }
    }

    co_return locations;
}

async::Task<> Server::onGotoDeclaration(json::Value id, const proto::DeclarationParams& params) {
    auto locations =
        co_await lookup(params, RelationKind(RelationKind::Declaration, RelationKind::Definition));
    co_await response(std::move(id), json::serialize(locations));
    co_return;
}

async::Task<> Server::onGotoDefinition(json::Value id, const proto::DefinitionParams& params) {
    auto locations = co_await lookup(params, RelationKind::Definition);
    co_await response(std::move(id), json::serialize(locations));
    co_return;
}

async::Task<> Server::onGotoTypeDefinition(json::Value id,
                                           const proto::TypeDefinitionParams& params) {
    auto locations = co_await lookup(params, RelationKind::TypeDefinition);
    co_await response(std::move(id), json::serialize(locations));
    co_return;
}

async::Task<> Server::onGotoImplementation(json::Value id,
                                           const proto::ImplementationParams& params) {
    auto locations = co_await lookup(params, RelationKind::Implementation);
    co_await response(std::move(id), json::serialize(locations));
    co_return;
}

async::Task<> Server::onFindReferences(json::Value id, const proto::ReferenceParams& params) {
    auto locations = co_await lookup(
        params,
        RelationKind(RelationKind::Declaration, RelationKind::Definition, RelationKind::Reference));
    co_return;
}

async::Task<> Server::onPrepareHierarchy(json::Value id,
                                         const proto::CallHierarchyPrepareParams& params) {
    auto path = SourceConverter::toPath(params.textDocument.uri);
    auto content = co_await getFileContent(path);
    if(content.empty()) {
        response(std::move(id), json::Value(nullptr));
        co_return;
    }

    auto offset = SC.toOffset(content, params.position);

    proto::CallHierarchyPrepareResult result;

    co_await indexer.resolve(
        path,
        offset,
        [&](llvm::StringRef path, const index::SymbolIndex::Symbol& symbol) {
            for(auto relation: symbol.relations()) {
                if(relation.kind().is_one_of(RelationKind::Declaration, RelationKind::Definition)) {
                    result.emplace_back(proto::CallHierarchyItem{
                        .name = symbol.name().str(),
                        .uri = SourceConverter::toURI(path),
                        .range = SC.toRange(*relation.range(), content),
                        .selectionRange = SC.toRange(*relation.symbolRange(), content),
                        .data = symbol.id(),
                    });
                }
            }
        });

    co_return;
}

async::Task<> Server::onIncomingCall(json::Value id,
                                     const proto::CallHierarchyIncomingCallsParams& params) {
    co_return;
}

async::Task<> Server::onOutgoingCall(json::Value id,
                                     const proto::CallHierarchyOutgoingCallsParams& params) {
    co_return;
}

async::Task<> Server::onSupertypes(json::Value id,
                                   const proto::TypeHierarchySupertypesParams& params) {
    co_return;
}

async::Task<> Server::onSubtypes(json::Value id, const proto::TypeHierarchySubtypesParams& params) {
    co_return;
}

async::Task<> Server::onDocumentHighlight(json::Value id,
                                          const proto::DocumentHighlightParams& params) {
    co_return;
}

async::Task<> Server::onDocumentLink(json::Value id, const proto::DocumentLinkParams& params) {
    co_return;
}

async::Task<> Server::onHover(json::Value id, const proto::HoverParams& params) {
    co_return;
}

async::Task<> Server::onCodeLens(json::Value id, const proto::CodeLensParams& params) {
    co_return;
}

async::Task<> Server::onFoldingRange(json::Value id, const proto::FoldingRangeParams& params) {
    co_return;
}

async::Task<> Server::onDocumentSymbol(json::Value id, const proto::DocumentSymbolParams& params) {
    co_return;
}

async::Task<> Server::onSemanticTokens(json::Value id, const proto::SemanticTokensParams& params) {
    auto path = SourceConverter::toPath(params.textDocument.uri);
    auto tokens = co_await indexer.semanticTokens(path);
    co_await response(std::move(id), json::serialize(tokens));
}

async::Task<> Server::onInlayHint(json::Value id, const proto::InlayHintParams& params) {
    co_return;
}

async::Task<> Server::onCodeCompletion(json::Value id, const proto::CompletionParams& params) {
    // auto path = URI::resolve(params.textDocument.uri);
    // async::response(std::move(id), json::serialize(result));
    co_return;
}

async::Task<> Server::onSignatureHelp(json::Value id, const proto::SignatureHelpParams& params) {
    co_return;
}

async::Task<> Server::onCodeAction(json::Value id, const proto::CodeActionParams& params) {
    co_return;
}

async::Task<> Server::onFormatting(json::Value id, const proto::DocumentFormattingParams& params) {
    co_return;
}

async::Task<> Server::onRangeFormatting(json::Value id,
                                        const proto::DocumentRangeFormattingParams& params) {
    co_return;
}

}  // namespace clice
