//===-- CDInstrInfo.cpp - CD VM instruction information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDInstrInfo.h"
#include "CDSubtarget.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "CDGenInstrInfo.inc"

using namespace llvm;

CDInstrInfo::CDInstrInfo(const CDSubtarget &STI)
    : CDGenInstrInfo(STI, RI) {}

void CDInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator I,
                              const DebugLoc &DL, Register DestReg,
                              Register SrcReg, bool KillSrc, bool,
                              bool) const {
  BuildMI(MBB, I, DL, get(CD::CD_MOVE), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}
