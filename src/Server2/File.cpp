#include "Server/File.h"

namespace clice {

async2::Task<> FileController::open(llvm::StringRef file) {
    /// 首先根据 rule 来对 file 进行判别

    /// 查看一下这 file 属于什么模式

    /// 1. 正常的源文件或者模块文件

    /// 预处理一遍源文件，获取一些相关的信息
    /// 是不是 module file?
    /// 获取模块依赖关系
    /// 或者计算 Preamble 的位置

    auto command = database.getCommand(file);

    /// 如果不是 readonly 模式
    /// 调用 CacheController 里面对应的函数更新这个文件的 cache
    /// PCH or PCM

    /// 准备 CompilationParams 进行 AST 编译

    /// 调度索引任务 ...

    /// 2. 头文件
    
    /// 先检查一下该头文件是否有选中的上下文... [一组 include 位置]
    /// 
    /// 对上下文源文件进行上述的处理 

    ///
    /// ...

    /// 重新索引依赖这个头文件的源文件

    co_return;
}

}  // namespace clice
