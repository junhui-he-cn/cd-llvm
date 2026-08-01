//===-- CDMCTargetDesc.cpp - CD MC target descriptions ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../TargetInfo/CDTargetInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#define GET_INSTRINFO_ENUM
#include "CDGenInstrInfo.inc"

#define GET_REGINFO_ENUM
#include "CDGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "CDGenSubtargetInfo.inc"

#define GET_INSTRINFO_MC_DESC
#include "CDGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "CDGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "CDGenRegisterInfo.inc"

using namespace llvm;

namespace {
class CDMCAsmInfo final : public MCAsmInfo {
public:
  explicit CDMCAsmInfo(const Triple &, const MCTargetOptions &Options)
      : MCAsmInfo(Options) {}
};
} // namespace

static MCInstrInfo *createCDMCInstrInfo() {
  MCInstrInfo *Info = new MCInstrInfo();
  InitCDMCInstrInfo(Info);
  return Info;
}

static MCRegisterInfo *createCDMCRegisterInfo(const Triple &) {
  MCRegisterInfo *Info = new MCRegisterInfo();
  InitCDMCRegisterInfo(Info, CD::R0);
  return Info;
}

static MCSubtargetInfo *createCDMCSubtargetInfo(const Triple &TT,
                                                 StringRef CPU,
                                                 StringRef FS) {
  return createCDMCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeCDTargetMC() {
  Target &T = getTheCDTarget();
  RegisterMCAsmInfo<CDMCAsmInfo> X(T);
  TargetRegistry::RegisterMCInstrInfo(T, createCDMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createCDMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createCDMCSubtargetInfo);
}
