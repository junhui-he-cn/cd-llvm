//===-- CDMCTargetDesc.cpp - CD MC target descriptions ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../TargetInfo/CDTargetInfo.h"
#include "llvm/ADT/StringTable.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

namespace {
class CDMCAsmInfo final : public MCAsmInfo {
public:
  explicit CDMCAsmInfo(const Triple &, const MCTargetOptions &Options)
      : MCAsmInfo(Options) {}
};
} // namespace

static MCInstrInfo *createCDMCInstrInfo() { return new MCInstrInfo(); }

static MCRegisterInfo *createCDMCRegisterInfo(const Triple &) {
  return new MCRegisterInfo();
}

static MCSubtargetInfo *createCDMCSubtargetInfo(const Triple &TT,
                                                 StringRef CPU,
                                                 StringRef FS) {
  return new MCSubtargetInfo(TT, CPU, CPU, FS, StringTable("\0"), {}, {},
                             nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeCDTargetMC() {
  Target &T = getTheCDTarget();
  RegisterMCAsmInfo<CDMCAsmInfo> X(T);
  TargetRegistry::RegisterMCInstrInfo(T, createCDMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createCDMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createCDMCSubtargetInfo);
}
