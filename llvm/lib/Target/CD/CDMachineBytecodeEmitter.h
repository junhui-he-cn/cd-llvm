//===-- CDMachineBytecodeEmitter.h - CD machine artifact pass -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDMACHINEBYTECODEEMITTER_H
#define LLVM_LIB_TARGET_CD_CDMACHINEBYTECODEEMITTER_H

namespace llvm {
class ModulePass;
class raw_ostream;

ModulePass *createCDMachineBytecodeEmitterPass(raw_ostream &OS);
} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDMACHINEBYTECODEEMITTER_H
