//===-- CD.td - Describe the CD bytecode target ------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

include "CDRegisterInfo.td"
include "CDCallingConv.td"
include "CDSubtarget.td"
include "CDInstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<CDValue>;

def CDInstrInfo : InstrInfo;

def CD : Target {
  let InstructionSet = CDInstrInfo;
}
