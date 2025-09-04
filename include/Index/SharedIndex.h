#pragma once

#include <vector>
#include "AST/SourceCode.h"
#include "llvm/ADT/DenseMap.h"
#define ROARING_EXCEPTIONS 0
#include "roaring/roaring.hh"

namespace clice::index {

/// 有五种级别的索引
/// 1.
/// 索引源文件直接产生的结果，里面是源文件中各个文件的索引，和包含图，以及源文件里面所有的外部符号的定义
/// 2. 源文件索引里面的分片索引，是每个文件中出现的 occurrences 和 relations 等
/// 3. 头文件索引，来自于合并多个源文件产生的同一个头文件的分片索引
/// 4. 源文件索引，源文件对应的分片索引和包含图
/// 5. 全局索引，储存了整个项目中所有的外部符号定义，以及哪些索引文件中有它们的引用

using SymbolID = std::uint64_t;

struct Relation {};

struct Symbol {
    SymbolID id;
    std::vector<Relation> relation;
};

struct Occurrence {
    LocalSourceRange range;
    SymbolID id;
};

template <typename K, typename V>
struct flat_map {};

using Bitmap = roaring::Roaring;

/// 首要问题，当一个的分片索引来的时候，我们怎么处理呢？
/// 首先计算这个分片索引里面所有元素的 hash 以及整体 hash
/// 如果有现有的分片索引和它相同，那么它俩可以共用一个 id
/// 如果不同，给它分配一个新的 id，然后把所有元素插入到现有的，并且更新 bitmap
/// 第二个问题，如果要更新一个旧得分片索引怎么做呢？其实就是是删除之前的，然后在添加新的
/// 怎么删除之前的呢？找到这个文件对应的文件 id 遍历把 bitmap 全清了就行了，遍历所有的
/// id 这个操作可能比较昂贵，建议合批次做
///
/// 问题又来了，元素应当是有序数组吗？没必要，注意我们这里定义的是内存结构 ...
/// 但是其实也不是原始的内存结构，用不用数组好像都行？ .... 并非如此
///
struct HeaderContext {
    std::string source;
    std::uint32_t include;
    std::uint32_t fid;
};

struct HeaderIndex {
    /// 储存每个 fid 对应的 index hash 值，如果变动，则需要添加新的...
    llvm::DenseMap<std::uint64_t, std::uint32_t> hashs;

    /// 储存所有的头文件上下文 ....
    std::vector<HeaderContext> contexts;
};

struct UnitIndex {
    flat_map<SymbolID, std::vector<Relation>> relations;

    flat_map<LocalSourceRange, std::vector<SymbolID>> occurrences;

    /// The local symbols
    llvm::DenseMap<SymbolID, Symbol> local_symbols;
};

struct SymbolIndex {};

}  // namespace clice::index
