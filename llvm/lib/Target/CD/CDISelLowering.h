//===-- CDISelLowering.h - CD VM DAG lowering ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDISELLOWERING_H
#define LLVM_LIB_TARGET_CD_CDISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class CDSubtarget;

class CDTargetLowering final : public TargetLowering {
public:
  CDTargetLowering(const TargetMachine &TM, const CDSubtarget &STI);

  bool areJTsAllowed(const Function *) const override { return false; }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDISELLOWERING_H
