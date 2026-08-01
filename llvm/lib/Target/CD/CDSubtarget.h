//===-- CDSubtarget.h - CD VM subtarget information ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDSUBTARGET_H
#define LLVM_LIB_TARGET_CD_CDSUBTARGET_H

#include "CDFrameLowering.h"
#include "CDISelLowering.h"
#include "CDInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SUBTARGETINFO_HEADER
#include "CDGenSubtargetInfo.inc"

namespace llvm {

class CDSubtarget final : public CDGenSubtargetInfo {
  CDInstrInfo InstrInfo;
  CDFrameLowering FrameLowering;
  CDTargetLowering TLInfo;

public:
  CDSubtarget(const Triple &TT, const std::string &CPU,
              const std::string &FS, const TargetMachine &TM);

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU,
                              StringRef FS);

  const CDInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const CDFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const CDTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const CDRegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDSUBTARGET_H
