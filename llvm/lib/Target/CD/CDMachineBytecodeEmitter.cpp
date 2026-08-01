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
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

namespace {

using CDConstant = cd::CDConstant;
using CDInstruction = cd::CDInstruction;
using CDOpcode = cd::CDOpcode;
using CDBody = cd::CDBody;

static bool isScalarType(const Type *Type) {
  return Type->isIntegerTy() || Type->isFloatingPointTy();
}

static bool isSupportedValue(const Value *Value) {
  return isScalarType(Value->getType()) || isa<ConstantPointerNull>(Value);
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
  StringMap<unsigned> NameIndexes;
  DenseMap<const Function *, unsigned> FunctionIndexes;
  DenseMap<const Constant *, Register> ConstantRegisters;
  DenseMap<const Function *, Register> FunctionRegisters;
  DenseMap<const Value *, Register> ValueRegisters;
  DenseMap<const AllocaInst *, unsigned> AllocaNames;
  std::vector<std::string> ParameterNames;
  std::set<std::string> UsedStorageNames;
  unsigned AllocaSerial = 0;

  unsigned addName(StringRef Name) {
    auto It = NameIndexes.find(Name);
    if (It != NameIndexes.end())
      return It->second;

    const unsigned Index = Artifact.names.size();
    Artifact.names.push_back(Name.str());
    NameIndexes[Artifact.names.back()] = Index;
    return Index;
  }

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

  Register createValueRegister(MachineRegisterInfo &MRI, const Value *Value) {
    Register Result = MRI.createVirtualRegister(&CD::CDValueRegClass);
    ValueRegisters[Value] = Result;
    return Result;
  }

  Register materializeConstant(const Constant *ConstantValue,
                               MachineRegisterInfo &MRI,
                               MachineBasicBlock &MBB,
                               const TargetInstrInfo &TII) {
    auto It = ConstantRegisters.find(ConstantValue);
    if (It != ConstantRegisters.end())
      return It->second;

    Register Result = createValueRegister(MRI, ConstantValue);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_CONSTANT), Result)
        .addImm(addConstant(*ConstantValue));
    ConstantRegisters[ConstantValue] = Result;
    return Result;
  }

  Register materializeNil(MachineRegisterInfo &MRI, MachineBasicBlock &MBB,
                          const TargetInstrInfo &TII) {
    const auto *Nil =
        ConstantPointerNull::get(PointerType::getUnqual(M.getContext()));
    return materializeConstant(Nil, MRI, MBB, TII);
  }

  Register materializeFunction(const Function *FunctionValue,
                               MachineRegisterInfo &MRI,
                               MachineBasicBlock &MBB,
                               const TargetInstrInfo &TII) {
    auto It = FunctionRegisters.find(FunctionValue);
    if (It != FunctionRegisters.end())
      return It->second;

    auto FunctionIndex = FunctionIndexes.find(FunctionValue);
    if (FunctionIndex == FunctionIndexes.end())
      unsupported("a call to an undefined, declared, or @main function");

    Register Result = createValueRegister(MRI, FunctionValue);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_MAKE_FUNCTION), Result)
        .addImm(FunctionIndex->second);
    FunctionRegisters[FunctionValue] = Result;
    return Result;
  }

  std::string uniqueStorageName(StringRef Base) {
    std::string Name = Base.str();
    if (Name.empty())
      Name = "alloca" + std::to_string(AllocaSerial++);
    if (!UsedStorageNames.insert(Name).second) {
      const std::string Original = Name;
      unsigned Suffix = 1;
      do {
        Name = Original + "#" + std::to_string(Suffix++);
      } while (!UsedStorageNames.insert(Name).second);
    }
    return Name;
  }

  void prepareStorage(Function &F) {
    for (BasicBlock &BB : F)
      for (Instruction &Instruction : BB) {
        const auto *Alloca = dyn_cast<AllocaInst>(&Instruction);
        if (!Alloca)
          continue;

        const auto *ArraySize = dyn_cast<ConstantInt>(Alloca->getArraySize());
        if (!ArraySize || !ArraySize->isOne() ||
            !isScalarType(Alloca->getAllocatedType()))
          unsupported("an unsupported alloca shape");
        const std::string Name = uniqueStorageName(
            Alloca->hasName() ? Alloca->getName() : StringRef());
        AllocaNames[Alloca] = addName(Name);
      }
  }

  Register valueRegister(const Value *Value, MachineRegisterInfo &MRI,
                         MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (const auto *ConstantValue = dyn_cast<Constant>(Value))
      return materializeConstant(ConstantValue, MRI, MBB, TII);

    auto It = ValueRegisters.find(Value);
    if (It != ValueRegisters.end())
      return It->second;
    unsupported("an unassigned or non-scalar SSA value");
  }

  static unsigned binaryOpcode(const Instruction &I) {
    switch (I.getOpcode()) {
    case Instruction::Add:
    case Instruction::FAdd:
      return CD::CD_ADD;
    case Instruction::Sub:
    case Instruction::FSub:
      return CD::CD_SUBTRACT;
    case Instruction::Mul:
    case Instruction::FMul:
      return CD::CD_MULTIPLY;
    case Instruction::SDiv:
    case Instruction::FDiv:
      return CD::CD_DIVIDE;
    case Instruction::UDiv:
      unsupported("unsigned integer division");
    default:
      unsupported("an unsupported binary instruction");
    }
  }

  void lowerBinary(const BinaryOperator &Binary, MachineRegisterInfo &MRI,
                   MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (!isScalarType(Binary.getType()) ||
        Binary.getType()->isIntegerTy(1))
      unsupported("a boolean or non-scalar binary operation");

    Register Left = valueRegister(Binary.getOperand(0), MRI, MBB, TII);
    Register Right = valueRegister(Binary.getOperand(1), MRI, MBB, TII);
    Register Result = createValueRegister(MRI, &Binary);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(binaryOpcode(Binary)), Result)
        .addReg(Left)
        .addReg(Right);
  }

  static bool isTrueBoolean(const Value *Value) {
    const auto *Constant = dyn_cast<ConstantInt>(Value);
    return Constant && Constant->getType()->isIntegerTy(1) &&
           Constant->isOne();
  }

  void lowerNot(const BinaryOperator &Binary, MachineRegisterInfo &MRI,
                MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    const Value *Source = nullptr;
    if (isTrueBoolean(Binary.getOperand(0)))
      Source = Binary.getOperand(1);
    else if (isTrueBoolean(Binary.getOperand(1)))
      Source = Binary.getOperand(0);
    else
      unsupported("an unsupported boolean XOR operation");

    if (!Source->getType()->isIntegerTy(1))
      unsupported("a non-boolean XOR operand");

    Register SourceRegister = valueRegister(Source, MRI, MBB, TII);
    Register Result = createValueRegister(MRI, &Binary);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_NOT), Result)
        .addReg(SourceRegister);
  }

  void lowerCast(const CastInst &Cast, MachineRegisterInfo &MRI,
                 MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (!isScalarType(Cast.getType()) ||
        !isScalarType(Cast.getOperand(0)->getType()))
      unsupported("a non-scalar cast");

    Register Source = valueRegister(Cast.getOperand(0), MRI, MBB, TII);
    Register Result = createValueRegister(MRI, &Cast);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_MOVE), Result)
        .addReg(Source);
  }

  void lowerCall(const CallInst &Call, MachineRegisterInfo &MRI,
                 MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    Function *Callee = Call.getCalledFunction();
    if (!Callee || Callee->isDeclaration() || Callee->isIntrinsic() ||
        FunctionIndexes.find(Callee) == FunctionIndexes.end())
      unsupported("a call to an undefined, declared, intrinsic, or @main function");
    if (Call.arg_size() != Callee->arg_size())
      unsupported("a function call with mismatched arity");
    for (const Use &Argument : Call.args())
      if (!isSupportedValue(Argument.get()))
        unsupported("a non-scalar function call argument");

    Register Result = createValueRegister(MRI, &Call);
    Register CalleeRegister =
        materializeFunction(Callee, MRI, MBB, TII);
    std::vector<Register> Arguments;
    for (const Use &Argument : Call.args())
      Arguments.push_back(valueRegister(Argument.get(), MRI, MBB, TII));

    MachineInstrBuilder CallBuilder =
        BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_CALL), Result)
            .addReg(CalleeRegister);
    for (Register Argument : Arguments)
      CallBuilder.addReg(Argument);
  }

  unsigned allocaName(const Value *Pointer) const {
    const auto *Alloca = dyn_cast<AllocaInst>(Pointer);
    if (!Alloca)
      unsupported("an indirect load or store");
    auto It = AllocaNames.find(Alloca);
    if (It == AllocaNames.end())
      unsupported("an unknown alloca value");
    return It->second;
  }

  void lowerLoad(const LoadInst &Load, MachineRegisterInfo &MRI,
                 MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (Load.isVolatile() || Load.isAtomic() ||
        !isScalarType(Load.getType()))
      unsupported("an unsupported load");

    Register Result = createValueRegister(MRI, &Load);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_LOAD_VAR), Result)
        .addImm(allocaName(Load.getPointerOperand()));
  }

  void lowerStore(const StoreInst &Store, MachineRegisterInfo &MRI,
                  MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (Store.isVolatile() || Store.isAtomic() ||
        !isSupportedValue(Store.getValueOperand()))
      unsupported("an unsupported store");

    Register Source =
        valueRegister(Store.getValueOperand(), MRI, MBB, TII);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_STORE_VAR))
        .addImm(allocaName(Store.getPointerOperand()))
        .addReg(Source);
  }

  void lowerFNeg(const Instruction &Instruction, MachineRegisterInfo &MRI,
                 MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (!isScalarType(Instruction.getType()))
      unsupported("a non-scalar floating-point negation");

    Register Source =
        valueRegister(Instruction.getOperand(0), MRI, MBB, TII);
    Register Result = createValueRegister(MRI, &Instruction);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_NEGATE), Result)
        .addReg(Source);
  }

  static unsigned comparisonOpcode(const CmpInst &Compare) {
    switch (Compare.getPredicate()) {
    case CmpInst::ICMP_EQ:
    case CmpInst::FCMP_OEQ:
    case CmpInst::FCMP_UEQ:
      return CD::CD_EQUAL;
    case CmpInst::ICMP_NE:
    case CmpInst::FCMP_ONE:
    case CmpInst::FCMP_UNE:
      return CD::CD_NOT_EQUAL;
    case CmpInst::ICMP_SGT:
    case CmpInst::FCMP_OGT:
    case CmpInst::FCMP_UGT:
      return CD::CD_GREATER;
    case CmpInst::ICMP_SGE:
    case CmpInst::FCMP_OGE:
    case CmpInst::FCMP_UGE:
      return CD::CD_GREATER_EQUAL;
    case CmpInst::ICMP_SLT:
    case CmpInst::FCMP_OLT:
    case CmpInst::FCMP_ULT:
      return CD::CD_LESS;
    case CmpInst::ICMP_SLE:
    case CmpInst::FCMP_OLE:
    case CmpInst::FCMP_ULE:
      return CD::CD_LESS_EQUAL;
    case CmpInst::ICMP_UGT:
    case CmpInst::ICMP_UGE:
    case CmpInst::ICMP_ULT:
    case CmpInst::ICMP_ULE:
      unsupported("unsigned integer ordering");
    default:
      unsupported("an unsupported comparison predicate");
    }
  }

  void lowerCompare(const CmpInst &Compare, MachineRegisterInfo &MRI,
                    MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (!isScalarType(Compare.getOperand(0)->getType()) ||
        !isScalarType(Compare.getOperand(1)->getType()))
      unsupported("a non-scalar comparison");

    Register Left = valueRegister(Compare.getOperand(0), MRI, MBB, TII);
    Register Right = valueRegister(Compare.getOperand(1), MRI, MBB, TII);
    Register Result = createValueRegister(MRI, &Compare);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(comparisonOpcode(Compare)),
            Result)
        .addReg(Left)
        .addReg(Right);
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

  CDBody lowerFunction(Function &F, bool IsMain) {
    ValueRegisters.clear();
    ConstantRegisters.clear();
    FunctionRegisters.clear();
    AllocaNames.clear();
    ParameterNames.clear();
    UsedStorageNames.clear();
    AllocaSerial = 0;

    if (IsMain && F.getName() != "main")
      unsupported("a non-main function as the entry body");
    if (F.isDeclaration())
      unsupported("a declared function body");
    if (IsMain && F.arg_size() != 0)
      unsupported("@main parameters in the first machine slice");
    if (F.size() != 1)
      unsupported("multiple basic blocks in the first machine slice");

    MachineFunction &MF = MMI.getOrCreateMachineFunction(F);
    if (MF.size() != 0)
      unsupported("pre-existing machine basic blocks");

    MachineBasicBlock *MBB = MF.CreateMachineBasicBlock(&F.getEntryBlock());
    MF.insert(MF.end(), MBB);
    MachineRegisterInfo &MRI = MF.getRegInfo();
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();

    unsigned AnonymousArgumentSerial = 0;
    for (Argument &Argument : F.args()) {
      if (!isScalarType(Argument.getType()))
        unsupported("a non-scalar function parameter");

      std::string Name = Argument.hasName()
                             ? Argument.getName().str()
                             : "arg" + std::to_string(AnonymousArgumentSerial++);
      if (!UsedStorageNames.insert(Name).second) {
        const std::string Original = Name;
        unsigned Suffix = 1;
        do {
          Name = Original + "#" + std::to_string(Suffix++);
        } while (!UsedStorageNames.insert(Name).second);
      }

      ParameterNames.push_back(Name);
      Register Result = createValueRegister(MRI, &Argument);
      BuildMI(*MBB, MBB->end(), DebugLoc(), TII.get(CD::CD_LOAD_VAR), Result)
          .addImm(addName(Name));
    }

    prepareStorage(F);

    for (const Instruction &Instruction : F.getEntryBlock()) {
      if (isa<DbgInfoIntrinsic>(&Instruction))
        continue;

      if (isa<AllocaInst>(&Instruction))
        continue;

      if (const auto *Load = dyn_cast<LoadInst>(&Instruction)) {
        lowerLoad(*Load, MRI, *MBB, TII);
        continue;
      }

      if (const auto *Store = dyn_cast<StoreInst>(&Instruction)) {
        lowerStore(*Store, MRI, *MBB, TII);
        continue;
      }

      if (const auto *Return = dyn_cast<ReturnInst>(&Instruction)) {
        const Value *ReturnValue = Return->getReturnValue();
        if (ReturnValue && !isSupportedValue(ReturnValue))
          unsupported("a non-scalar function return");
        Register Result = ReturnValue
                              ? valueRegister(ReturnValue, MRI, *MBB, TII)
                              : materializeNil(MRI, *MBB, TII);
        BuildMI(*MBB, MBB->end(), DebugLoc(), TII.get(CD::CD_RETURN))
            .addReg(Result);
        continue;
      }

      if (const auto *Binary = dyn_cast<BinaryOperator>(&Instruction)) {
        if (Binary->getOpcode() == Instruction::Xor)
          lowerNot(*Binary, MRI, *MBB, TII);
        else
          lowerBinary(*Binary, MRI, *MBB, TII);
        continue;
      }

      if (const auto *Compare = dyn_cast<CmpInst>(&Instruction)) {
        lowerCompare(*Compare, MRI, *MBB, TII);
        continue;
      }

      if (const auto *Cast = dyn_cast<CastInst>(&Instruction)) {
        lowerCast(*Cast, MRI, *MBB, TII);
        continue;
      }

      if (const auto *Call = dyn_cast<CallInst>(&Instruction)) {
        lowerCall(*Call, MRI, *MBB, TII);
        continue;
      }

      if (Instruction.getOpcode() == Instruction::FNeg) {
        lowerFNeg(Instruction, MRI, *MBB, TII);
        continue;
      }

      unsupported(Instruction.getOpcodeName());
    }

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
        case CD::CD_MAKE_FUNCTION:
          if (MI.getNumOperands() != 2 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isImm())
            unsupported("an invalid CD_MAKE_FUNCTION machine instruction");
          Body.instructions.push_back(CDInstruction::makeFunction(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              MI.getOperand(1).getImm()));
          break;
        case CD::CD_LOAD_VAR:
          if (MI.getNumOperands() != 2 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isImm())
            unsupported("an invalid CD_LOAD_VAR machine instruction");
          Body.instructions.push_back(CDInstruction::loadVar(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              MI.getOperand(1).getImm()));
          break;
        case CD::CD_STORE_VAR:
          if (MI.getNumOperands() != 2 || !MI.getOperand(0).isImm() ||
              !MI.getOperand(1).isReg())
            unsupported("an invalid CD_STORE_VAR machine instruction");
          Body.instructions.push_back(CDInstruction::storeVar(
              MI.getOperand(0).getImm(),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body)));
          break;
        case CD::CD_MOVE:
        case CD::CD_NEGATE:
        case CD::CD_NOT: {
          if (MI.getNumOperands() != 2 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg())
            unsupported("an invalid unary CD machine instruction");
          CDOpcode Opcode = CDOpcode::Move;
          if (MI.getOpcode() == CD::CD_NEGATE)
            Opcode = CDOpcode::Negate;
          else if (MI.getOpcode() == CD::CD_NOT)
            Opcode = CDOpcode::Not;
          Body.instructions.push_back(CDInstruction::unary(
              Opcode,
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body)));
          break;
        }
        case CD::CD_ADD:
        case CD::CD_SUBTRACT:
        case CD::CD_MULTIPLY:
        case CD::CD_DIVIDE:
        case CD::CD_EQUAL:
        case CD::CD_NOT_EQUAL:
        case CD::CD_GREATER:
        case CD::CD_GREATER_EQUAL:
        case CD::CD_LESS:
        case CD::CD_LESS_EQUAL: {
          if (MI.getNumOperands() != 3 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg())
            unsupported("an invalid binary CD machine instruction");
          CDOpcode Opcode = CDOpcode::Add;
          switch (MI.getOpcode()) {
          case CD::CD_SUBTRACT:
            Opcode = CDOpcode::Subtract;
            break;
          case CD::CD_MULTIPLY:
            Opcode = CDOpcode::Multiply;
            break;
          case CD::CD_DIVIDE:
            Opcode = CDOpcode::Divide;
            break;
          case CD::CD_EQUAL:
            Opcode = CDOpcode::Equal;
            break;
          case CD::CD_NOT_EQUAL:
            Opcode = CDOpcode::NotEqual;
            break;
          case CD::CD_GREATER:
            Opcode = CDOpcode::Greater;
            break;
          case CD::CD_GREATER_EQUAL:
            Opcode = CDOpcode::GreaterEqual;
            break;
          case CD::CD_LESS:
            Opcode = CDOpcode::Less;
            break;
          case CD::CD_LESS_EQUAL:
            Opcode = CDOpcode::LessEqual;
            break;
          default:
            break;
          }
          Body.instructions.push_back(CDInstruction::binary(
              Opcode,
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(2).getReg(), Registers, Body)));
          break;
        }
        case CD::CD_CALL: {
          if (MI.getNumOperands() < 2 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg())
            unsupported("an invalid CD_CALL machine instruction");
          std::vector<unsigned> Arguments;
          for (unsigned Index = 2; Index < MI.getNumOperands(); ++Index) {
            if (!MI.getOperand(Index).isReg())
              unsupported("an invalid CD_CALL argument operand");
            Arguments.push_back(artifactRegister(
                MI.getOperand(Index).getReg(), Registers, Body));
          }
          Body.instructions.push_back(CDInstruction::call(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body),
              std::move(Arguments)));
          break;
        }
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
    Body.parameterNames = ParameterNames;
    return Body;
  }

public:
  CDMachineModuleEmitter(Module &M, MachineModuleInfo &MMI, raw_ostream &OS)
      : M(M), MMI(MMI), OS(OS) {}

  void emit() {
    Function *Main = M.getFunction("main");
    if (!Main || Main->isDeclaration())
      unsupported("a module without a defined @main entry function");

    FunctionIndexes.clear();
    unsigned FunctionIndex = 0;
    for (Function &F : M)
      if (&F != Main && !F.isDeclaration() && !F.isIntrinsic())
        FunctionIndexes[&F] = FunctionIndex++;

    Artifact.main = lowerFunction(*Main, true);
    for (Function &F : M) {
      auto It = FunctionIndexes.find(&F);
      if (It == FunctionIndexes.end())
        continue;
      Artifact.functions.push_back(
          {F.getName().str(), static_cast<unsigned>(F.arg_size()),
           lowerFunction(F, false)});
    }

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
