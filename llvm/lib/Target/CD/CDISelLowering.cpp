//===-- CDISelLowering.cpp - CD VM DAG lowering --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDISelLowering.h"
#include "CDSubtarget.h"

using namespace llvm;

CDTargetLowering::CDTargetLowering(const TargetMachine &TM,
                                   const CDSubtarget &STI)
    : TargetLowering(TM, STI) {
  addRegisterClass(MVT::i64, &CD::CDValueRegClass);
  computeRegisterProperties(STI.getRegisterInfo());
  setBooleanContents(ZeroOrOneBooleanContent);
}
