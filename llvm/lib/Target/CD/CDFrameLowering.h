//===-- CDFrameLowering.h - CD VM frame lowering -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDFRAMELOWERING_H
#define LLVM_LIB_TARGET_CD_CDFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class CDSubtarget;

class CDFrameLowering final : public TargetFrameLowering {
public:
  explicit CDFrameLowering(const CDSubtarget &)
      : TargetFrameLowering(StackGrowsDown, Align(8), 0) {}

  void emitPrologue(MachineFunction &, MachineBasicBlock &) const override {}
  void emitEpilogue(MachineFunction &, MachineBasicBlock &) const override {}

protected:
  bool hasFPImpl(const MachineFunction &) const override { return false; }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDFRAMELOWERING_H
