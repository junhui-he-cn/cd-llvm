//===-- CDTargetInfo.cpp - CD target information -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

Target &llvm::getTheCDTarget() {
  static Target TheCDTarget;
  return TheCDTarget;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeCDTargetInfo() {
  RegisterTarget<Triple::cd, /*HasJIT=*/false> X(
      getTheCDTarget(), "cd", "Compiler Design bytecode", "CD");
}
