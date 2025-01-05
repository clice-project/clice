#pragma once

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FileUtilities.h>
#include <llvm/Support/MemoryBuffer.h>

// Query apple's `xcrun` launcher, which is the source of truth for "how should"
// clang be invoked on this system.
inline llvm::Expected<std::string> queryXcrun(llvm::ArrayRef<llvm::StringRef> Argv) {
    auto xcrun = llvm::sys::findProgramByName("xcrun");
    if(!xcrun)
        return llvm::createStringError(
            "Couldn't find xcrun. Hopefully you have a non-apple toolchain...");

    llvm::SmallString<64> outFile;
    auto errCode = llvm::sys::fs::createTemporaryFile("clangd-xcrun", "", outFile);
    if(errCode)
        return llvm::createStringError(errCode, "Can't create temporary file 'clangd-xcrun'");

    llvm::FileRemover outRemover(outFile);
    std::optional<llvm::StringRef> redirects[3] = {/*stdin=*/{""},
                                                   /*stdout=*/{outFile.str()},
                                                   /*stderr=*/{""}};
    int ret = llvm::sys::ExecuteAndWait(*xcrun,
                                        Argv,
                                        /*Env=*/std::nullopt,
                                        redirects,
                                        /*SecondsToWait=*/10);
    if(ret != 0)
        return llvm::createStringError(
            std::format(
                "xcrun exists but failed with code {0}. " "If you have a non-apple toolchain, this is OK." "Otherwise, try xcode-select --install.",
                ret));

    auto buf = llvm::MemoryBuffer::getFile(outFile);
    if(!buf)
        return llvm::createStringError(
            std::format("Can't read xcrun output: {0}", buf.getError().message()));

    llvm::StringRef path = buf->get()->getBuffer().trim();
    if(path.empty())
        return llvm::createStringError("xcrun produced no output");

    return path.str();
}
