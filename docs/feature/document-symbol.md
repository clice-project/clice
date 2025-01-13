# Document Symbol 

## Supported Case 
The full cases mentioned in LSP are listed in [enum clice::proto::SymbolKind::Kind](../../include/Feature/DocumentSymbol.h).   

In clice, the following symbols is included: 

1. Namespace
2. Free function declaration
3. Enum, and it's members
4. Struct, class, add it's fields or methods.    
Abstract class is specified as `Interface`. 

5. Non-local and non-parameter variables    
They were specified as `Constant` or `Variable`  based on their declaration. 


For all above case, a deprecated tag will be added if the item is marked with `[[deprecated]]`. 

