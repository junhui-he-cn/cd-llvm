//===-- CDMachineBytecodeEmitter.cpp - CD machine artifact pass ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDMachineBytecodeEmitter.h"
#include "CDBytecodeFormat.h"
#include "CDInstrInfo.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <cmath>
#include <string>
#include <utility>

using namespace llvm;

namespace {

using CDConstant = cd::CDConstant;
using CDInstruction = cd::CDInstruction;
using CDOpcode = cd::CDOpcode;
using CDBody = cd::CDBody;

static bool isScalarType(const Type *Type) {
  return Type->isIntegerTy() || Type->isFloatingPointTy();
}

[[noreturn]] static void unsupported(StringRef Message) {
  report_fatal_error(Twine("CD machine backend does not support ") + Message);
}

class CDMachineModuleEmitter {
  Module &M;
  MachineModuleInfo &MMI;
  raw_ostream &OS;
  cd::CDArtifact Artifact;
  StringMap<unsigned> ConstantIndexes;

  unsigned addConstant(const Constant &C) {
    CDConstant Value;
    std::string Key;

    if (const auto *CI = dyn_cast<ConstantInt>(&C)) {
      if (CI->getType()->isIntegerTy(1)) {
        Value = CDConstant::boolean(CI->isOne());
        Key = Value.text;
      } else {
        APFloat Converted(APFloat::IEEEdouble());
        if (Converted.convertFromAPInt(
                CI->getValue(), /*IsSigned=*/true,
                APFloat::rmNearestTiesToEven) != APFloat::opOK)
          unsupported("an integer constant that is not exactly representable");
        Value = CDConstant::number(Converted.convertToDouble());
        Key = Value.text;
      }
    } else if (const auto *CFP = dyn_cast<ConstantFP>(&C)) {
      const double Number = CFP->getValueAPF().convertToDouble();
      if (!std::isfinite(Number))
        unsupported("a non-finite floating-point constant");
      Value = CDConstant::number(Number);
      Key = Value.text;
    } else if (isa<ConstantPointerNull>(&C)) {
      Value = CDConstant::nil();
      Key = "nil";
    } else {
      unsupported("aggregate, undef, poison, or expression constants");
    }

    auto It = ConstantIndexes.find(Key);
    if (It != ConstantIndexes.end())
      return It->second;

    const unsigned Index = Artifact.constants.size();
    Artifact.constants.push_back(std::move(Value));
    ConstantIndexes[Key] = Index;
    return Index;
  }

  static unsigned artifactRegister(
      Register RegisterValue, DenseMap<unsigned, unsigned> &Registers,
      CDBody &Body) {
    if (!RegisterValue.isVirtual())
      unsupported("a physical register in the artifact bridge");

    const unsigned VirtualRegister = RegisterValue.virtRegIndex();
    auto It = Registers.find(VirtualRegister);
    if (It != Registers.end())
      return It->second;

    const unsigned Result = Body.registers++;
    Registers[VirtualRegister] = Result;
    return Result;
  }

  CDBody lowerMain() {
    Function *Main = M.getFunction("main");
    if (!Main || Main->isDeclaration())
      unsupported("a module without a defined @main entry function");
    if (Main->arg_size() != 0)
      unsupported("@main parameters in the first machine slice");

    MachineFunction &MF = MMI.getOrCreateMachineFunction(*Main);
    if (MF.size() != 0)
      unsupported("pre-existing machine basic blocks");

    MachineBasicBlock *MBB = MF.CreateMachineBasicBlock(&Main->getEntryBlock());
    MF.insert(MF.end(), MBB);
    MachineRegisterInfo &MRI = MF.getRegInfo();
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();

    const ReturnInst *Return = dyn_cast<ReturnInst>(Main->getEntryBlock().getTerminator());
    if (!Return || !Return->getReturnValue())
      unsupported("a non-constant @main return in the first machine slice");
    const auto *Value = dyn_cast<Constant>(Return->getReturnValue());
    if (!Value || !isScalarType(Value->getType()))
      unsupported("a non-scalar or non-constant @main return");

    const unsigned ConstantIndex = addConstant(*Value);
    Register Result = MRI.createVirtualRegister(&CD::CDValueRegClass);
    BuildMI(*MBB, MBB->end(), DebugLoc(), TII.get(CD::CD_CONSTANT), Result)
        .addImm(ConstantIndex);
    BuildMI(*MBB, MBB->end(), DebugLoc(), TII.get(CD::CD_RETURN))
        .addReg(Result);

    CDBody Body;
    DenseMap<unsigned, unsigned> Registers;
    for (const MachineBasicBlock &Block : MF) {
      for (const MachineInstr &MI : Block) {
        if (MI.isDebugInstr())
          continue;

        switch (MI.getOpcode()) {
        case CD::CD_CONSTANT:
          if (MI.getNumOperands() != 2 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isImm())
            unsupported("an invalid CD_CONSTANT machine instruction");
          Body.instructions.push_back(CDInstruction::constant(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              MI.getOperand(1).getImm()));
          break;
        case CD::CD_RETURN:
          if (MI.getNumOperands() != 1 || !MI.getOperand(0).isReg())
            unsupported("an invalid CD_RETURN machine instruction");
          Body.instructions.push_back(CDInstruction::returnValue(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body)));
          break;
        default:
          unsupported("an unimplemented machine opcode in the artifact bridge");
        }
      }
    }
    return Body;
  }

public:
  CDMachineModuleEmitter(Module &M, MachineModuleInfo &MMI, raw_ostream &OS)
      : M(M), MMI(MMI), OS(OS) {}

  void emit() {
    Artifact.main = lowerMain();
    std::string Error;
    if (!cd::validateArtifact(Artifact, Error))
      report_fatal_error(Twine("CD machine artifact validation failed: ") +
                         Error);
    cd::serializeArtifact(Artifact, OS);
  }
};

class CDMachineBytecodeEmitter final : public ModulePass {
  raw_ostream &OS;

public:
  static char ID;

  explicit CDMachineBytecodeEmitter(raw_ostream &OS) : ModulePass(ID), OS(OS) {}

  bool runOnModule(Module &M) override {
    MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
    CDMachineModuleEmitter(M, MMI, OS).emit();
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    AU.setPreservesAll();
  }
};

char CDMachineBytecodeEmitter::ID = 0;

} // namespace

ModulePass *llvm::createCDMachineBytecodeEmitterPass(raw_ostream &OS) {
  return new CDMachineBytecodeEmitter(OS);
}
