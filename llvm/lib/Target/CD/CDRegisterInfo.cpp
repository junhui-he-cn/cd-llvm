//===-- CDRegisterInfo.cpp - CD VM register information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDRegisterInfo.h"
#include "CDSubtarget.h"

#define GET_REGINFO_TARGET_DESC
#include "CDGenRegisterInfo.inc"

using namespace llvm;

CDRegisterInfo::CDRegisterInfo() : CDGenRegisterInfo(CD::R0) {}

const MCPhysReg *
CDRegisterInfo::getCalleeSavedRegs(const MachineFunction *) const {
  static const MCPhysReg CalleeSavedRegs[] = {0};
  return CalleeSavedRegs;
}

BitVector
CDRegisterInfo::getReservedRegs(const MachineFunction &) const {
  BitVector Reserved(getNumRegs());
  for (unsigned Register = 1; Register < getNumRegs(); ++Register)
    Reserved.set(Register);
  return Reserved;
}

bool CDRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator, int,
                                         unsigned, RegScavenger *) const {
  return false;
}

Register CDRegisterInfo::getFrameRegister(const MachineFunction &) const {
  return CD::R0;
}
