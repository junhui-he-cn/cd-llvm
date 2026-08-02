//===-- CDTargetMachine.cpp - CD target machine --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDTargetMachine.h"
#include "CDBytecodeEmitter.h"
#include "CDMachineBytecodeEmitter.h"
#include "CDSubtarget.h"
#include "TargetInfo/CDTargetInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Target/TargetLoweringObjectFile.h"

using namespace llvm;

namespace {
enum class CDBackend { Direct, Machine };

static cl::opt<CDBackend> CDBackendOption(
    "cd-backend", cl::Hidden,
    cl::desc("Select the CD bytecode lowering backend"),
    cl::init(CDBackend::Direct),
    cl::values(clEnumValN(CDBackend::Direct, "direct",
                          "Use the direct LLVM IR emitter"),
               clEnumValN(CDBackend::Machine, "machine",
                          "Use the TableGen-backed machine emitter")));

enum class CDArtifactModeOption { Program, Module };

static cl::opt<CDArtifactModeOption> CDArtifactModeOptionValue(
    "cd-artifact", cl::Hidden,
    cl::desc("Select the CD bytecode artifact envelope"),
    cl::init(CDArtifactModeOption::Program),
    cl::values(clEnumValN(CDArtifactModeOption::Program, "program",
                          "Emit the linked-program envelope"),
               clEnumValN(CDArtifactModeOption::Module, "module",
                          "Emit the module-product envelope")));
} // namespace

namespace {
class CDTargetObjectFile final : public TargetLoweringObjectFile {
public:
  MCSection *getExplicitSectionGlobal(const GlobalObject *, SectionKind,
                                      const TargetMachine &) const override {
    return nullptr;
  }

protected:
  MCSection *SelectSectionForGlobal(const GlobalObject *, SectionKind,
                                    const TargetMachine &) const override {
    return nullptr;
  }
};
} // namespace

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeCDTarget() {
  RegisterTargetMachine<CDTargetMachine> X(getTheCDTarget());
}

CDTargetMachine::CDTargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(
          T, "e-p:64:64-i64:64-n8:16:32:64-S128", TT, CPU, FS, Options,
          Reloc::Static, CodeModel::Small, OL),
      TLOF(std::make_unique<CDTargetObjectFile>()) {
  initAsmInfo();
}

bool CDTargetMachine::addPassesToEmitFile(
    PassManagerBase &PM, raw_pwrite_stream &Out, raw_pwrite_stream *,
    CodeGenFileType FileType, bool,
    MachineModuleInfoWrapperPass *MMIWP) {
  if (FileType != CodeGenFileType::AssemblyFile)
    return true;

  const cd::CDArtifactMode ArtifactMode =
      CDArtifactModeOptionValue == CDArtifactModeOption::Module
          ? cd::CDArtifactMode::Module
          : cd::CDArtifactMode::Program;

  if (CDBackendOption == CDBackend::Machine) {
    if (!MMIWP)
      MMIWP = new MachineModuleInfoWrapperPass(this);
    PM.add(MMIWP);
    PM.add(createCDMachineBytecodeEmitterPass(Out, ArtifactMode));
    return false;
  }

  PM.add(createCDBytecodeEmitterPass(Out, ArtifactMode));
  return false;
}

const TargetSubtargetInfo *
CDTargetMachine::getSubtargetImpl(const Function &) const {
  if (!Subtarget) {
    const StringRef CPU = getTargetCPU();
    const std::string CPUName = CPU.empty() ? "generic" : CPU.str();
    Subtarget = std::make_unique<CDSubtarget>(
        getTargetTriple(), CPUName, getTargetFeatureString().str(), *this);
  }
  return Subtarget.get();
}
