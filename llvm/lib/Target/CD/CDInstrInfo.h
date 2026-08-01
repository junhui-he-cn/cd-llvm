//===-- CDInstrInfo.h - CD VM instruction information --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDINSTRINFO_H
#define LLVM_LIB_TARGET_CD_CDINSTRINFO_H

#include "CD.h"
#include "CDRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "CDGenInstrInfo.inc"

namespace llvm {

class CDSubtarget;

class CDInstrInfo : public CDGenInstrInfo {
  const CDRegisterInfo RI;

public:
  explicit CDInstrInfo(const CDSubtarget &STI);

  const CDRegisterInfo &getRegisterInfo() const { return RI; }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDINSTRINFO_H
