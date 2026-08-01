//===-- CDSubtarget.cpp - CD VM subtarget information --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDSubtarget.h"

#define GET_SUBTARGETINFO_TARGET_DESC
#include "CDGenSubtargetInfo.inc"

#define GET_SUBTARGETINFO_CTOR
#include "CDGenSubtargetInfo.inc"

using namespace llvm;

CDSubtarget::CDSubtarget(const Triple &TT, const std::string &CPU,
                         const std::string &FS, const TargetMachine &TM)
    : CDGenSubtargetInfo(TT, CPU, CPU, FS), InstrInfo(*this),
      FrameLowering(*this), TLInfo(TM, *this) {
  ParseSubtargetFeatures(CPU, CPU, FS);
}
