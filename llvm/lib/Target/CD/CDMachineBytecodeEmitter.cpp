//===-- CDMachineBytecodeEmitter.cpp - CD machine artifact pass ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDMachineBytecodeEmitter.h"
#include "CDBytecodeFormat.h"
#include "CDDebugInfo.h"
#include "CDInstrInfo.h"
#include "CDValueABI.h"
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
#include <optional>
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

static bool isSupportedPrintValue(const Value *Value) {
  return isSupportedValue(Value) || cd::isCDValue(*Value);
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
  DenseMap<const Value *, Register> ValueRegisters;
  DenseMap<const AllocaInst *, unsigned> AllocaNames;
  DenseMap<const PHINode *, unsigned> PhiNames;
  DenseMap<const BasicBlock *, MachineBasicBlock *> MachineBlocks;
  std::vector<std::string> ParameterNames;
  std::set<std::string> UsedStorageNames;
  unsigned AllocaSerial = 0;
  unsigned ValueSerial = 0;
  DebugLoc CurrentDebugLoc;

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

  unsigned addString(StringRef Value) {
    const std::string Key = "string:" + Value.str();
    auto It = ConstantIndexes.find(Key);
    if (It != ConstantIndexes.end())
      return It->second;

    const unsigned Index = Artifact.constants.size();
    Artifact.constants.push_back(CDConstant::string(Value));
    ConstantIndexes[Key] = Index;
    return Index;
  }

  unsigned addNameOperand(const Value *Value, StringRef Operation) {
    std::string Error;
    std::optional<StringRef> Name = cd::getNameConstant(*Value, Error);
    if (!Name)
      unsupported((Operation + " " + Error).str());
    return addName(*Name);
  }

  Register createValueRegister(MachineRegisterInfo &MRI, const Value *Value) {
    auto It = ValueRegisters.find(Value);
    if (It != ValueRegisters.end())
      return It->second;
    Register Result = MRI.createVirtualRegister(&CD::CDValueRegClass);
    ValueRegisters[Value] = Result;
    return Result;
  }

  Register createTemporaryRegister(MachineRegisterInfo &MRI) {
    return MRI.createVirtualRegister(&CD::CDValueRegClass);
  }

  Register materializeConstant(const Constant *ConstantValue,
                               MachineRegisterInfo &MRI,
                               MachineBasicBlock &MBB,
                               const TargetInstrInfo &TII) {
    Register Result = createTemporaryRegister(MRI);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_CONSTANT), Result)
        .addImm(addConstant(*ConstantValue));
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
    auto FunctionIndex = FunctionIndexes.find(FunctionValue);
    if (FunctionIndex == FunctionIndexes.end())
      unsupported("a call to an undefined, declared, or @main function");

    Register Result = createTemporaryRegister(MRI);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(CD::CD_MAKE_FUNCTION), Result)
        .addImm(FunctionIndex->second);
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
        if (Alloca) {
          const auto *ArraySize =
              dyn_cast<ConstantInt>(Alloca->getArraySize());
          if (!ArraySize || !ArraySize->isOne() ||
              !isScalarType(Alloca->getAllocatedType()))
            unsupported("an unsupported alloca shape");
          const std::string Name = uniqueStorageName(
              Alloca->hasName() ? Alloca->getName() : StringRef());
          AllocaNames[Alloca] = addName(Name);
          continue;
        }

        const auto *Phi = dyn_cast<PHINode>(&Instruction);
        if (!Phi)
          continue;
        if (!isScalarType(Phi->getType()))
          unsupported("a non-scalar PHI node");
        const std::string Base = Phi->hasName()
                                     ? Phi->getName().str()
                                     : "value" + std::to_string(ValueSerial++);
        const std::string Name = uniqueStorageName(Base);
        PhiNames[Phi] = addName(Name);
      }
  }

  void preallocateValueRegisters(Function &F, MachineRegisterInfo &MRI) {
    for (BasicBlock &BB : F)
      for (Instruction &Instruction : BB) {
        if (isa<DbgInfoIntrinsic>(&Instruction) ||
            isa<AllocaInst>(&Instruction) || Instruction.getType()->isVoidTy())
          continue;
        if (!isScalarType(Instruction.getType()) &&
            !cd::isCDValue(Instruction))
          unsupported("a non-scalar instruction result");
        createValueRegister(MRI, &Instruction);
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
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(binaryOpcode(Binary)), Result)
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
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_NOT), Result)
        .addReg(SourceRegister);
  }

  void lowerCast(const CastInst &Cast, MachineRegisterInfo &MRI,
                 MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (!isScalarType(Cast.getType()) ||
        !isScalarType(Cast.getOperand(0)->getType()))
      unsupported("a non-scalar cast");

    Register Source = valueRegister(Cast.getOperand(0), MRI, MBB, TII);
    Register Result = createValueRegister(MRI, &Cast);
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_MOVE), Result)
        .addReg(Source);
  }

  void lowerCall(const CallInst &Call, MachineRegisterInfo &MRI,
                 MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    Function *Callee = Call.getCalledFunction();
    if (Callee && cd::isStringIntrinsic(Call)) {
      std::string Error;
      std::optional<StringRef> Value = cd::getStringConstant(Call, Error);
      if (!Value)
        unsupported(Error);
      Register Result = createValueRegister(MRI, &Call);
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_CONSTANT), Result)
          .addImm(addString(*Value));
      return;
    }

    if (Callee && cd::isNativeIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateNativeCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      const unsigned Name =
          addNameOperand(Call.getArgOperand(0), "llvm.cd.native");
      std::vector<Register> Arguments;
      Arguments.reserve(Call.arg_size() - 1);
      for (unsigned Index = 1; Index < Call.arg_size(); ++Index)
        Arguments.push_back(
            valueRegister(Call.getArgOperand(Index), MRI, MBB, TII));
      MachineInstrBuilder NativeBuilder =
          BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_NATIVE_CALL),
                  Result)
              .addImm(Name);
      for (Register Argument : Arguments)
        NativeBuilder.addReg(Argument);
      return;
    }

    if (Callee && cd::isArrayIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateArrayCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      std::vector<Register> Elements;
      Elements.reserve(Call.arg_size() - 1);
      for (unsigned Index = 1; Index < Call.arg_size(); ++Index)
        Elements.push_back(
            valueRegister(Call.getArgOperand(Index), MRI, MBB, TII));

      MachineInstrBuilder ArrayBuilder =
          BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_ARRAY), Result);
      for (Register Element : Elements)
        ArrayBuilder.addReg(Element);
      return;
    }

    if (Callee && cd::isMapIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateMapCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      std::vector<Register> KeyValueOperands;
      KeyValueOperands.reserve(Call.arg_size() - 1);
      for (unsigned Index = 1; Index < Call.arg_size(); ++Index)
        KeyValueOperands.push_back(
            valueRegister(Call.getArgOperand(Index), MRI, MBB, TII));

      MachineInstrBuilder MapBuilder =
          BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_MAP), Result);
      for (Register KeyValueOperand : KeyValueOperands)
        MapBuilder.addReg(KeyValueOperand);
      return;
    }

    if (Callee && cd::isStructIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateStructCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      int64_t TypeName = -1;
      if (isa<ConstantPointerNull>(Call.getArgOperand(0)))
        TypeName = -1;
      else
        TypeName =
            addNameOperand(Call.getArgOperand(0), "llvm.cd.struct");
      std::vector<std::pair<unsigned, Register>> Fields;
      Fields.reserve((Call.arg_size() - 2) / 2);
      for (unsigned Index = 2; Index < Call.arg_size(); Index += 2) {
        Fields.emplace_back(
            addNameOperand(Call.getArgOperand(Index), "llvm.cd.struct"),
            valueRegister(Call.getArgOperand(Index + 1), MRI, MBB, TII));
      }
      MachineInstrBuilder StructBuilder =
          BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_STRUCT), Result);
      StructBuilder.addImm(TypeName);
      for (const auto &[Name, Value] : Fields) {
        StructBuilder.addImm(Name);
        StructBuilder.addReg(Value);
      }
      return;
    }

    if (Callee && cd::isVariantIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateVariantCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      const unsigned EnumName =
          addNameOperand(Call.getArgOperand(0), "llvm.cd.variant");
      const unsigned VariantName =
          addNameOperand(Call.getArgOperand(1), "llvm.cd.variant");
      std::vector<Register> Payload;
      Payload.reserve(Call.arg_size() - 3);
      for (unsigned Index = 3; Index < Call.arg_size(); ++Index)
        Payload.push_back(
            valueRegister(Call.getArgOperand(Index), MRI, MBB, TII));
      MachineInstrBuilder VariantBuilder =
          BuildMI(MBB, MBB.end(), CurrentDebugLoc,
                  TII.get(CD::CD_VARIANT), Result)
              .addImm(EnumName)
              .addImm(VariantName);
      for (Register Value : Payload)
        VariantBuilder.addReg(Value);
      return;
    }

    if (Callee && cd::isVariantTagIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateVariantTagCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      Register Value = valueRegister(Call.getArgOperand(0), MRI, MBB, TII);
      const unsigned EnumName =
          addNameOperand(Call.getArgOperand(1), "llvm.cd.variant.tag");
      const unsigned VariantName =
          addNameOperand(Call.getArgOperand(2), "llvm.cd.variant.tag");
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_VARIANT_TAG), Result)
          .addReg(Value)
          .addImm(EnumName)
          .addImm(VariantName);
      return;
    }

    if (Callee && cd::isVariantFieldIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateVariantFieldCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      Register Value = valueRegister(Call.getArgOperand(0), MRI, MBB, TII);
      const auto *Index = cast<ConstantInt>(Call.getArgOperand(1));
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_VARIANT_FIELD), Result)
          .addReg(Value)
          .addImm(Index->getZExtValue());
      return;
    }

    if (Callee && cd::isFieldIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateFieldCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      Register Object = valueRegister(Call.getArgOperand(0), MRI, MBB, TII);
      const unsigned Name =
          addNameOperand(Call.getArgOperand(1), "llvm.cd.field");
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_FIELD), Result)
          .addReg(Object)
          .addImm(Name);
      return;
    }

    if (Callee && cd::isAssignFieldIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateAssignFieldCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      Register Object = valueRegister(Call.getArgOperand(0), MRI, MBB, TII);
      const unsigned Name =
          addNameOperand(Call.getArgOperand(1), "llvm.cd.assign.field");
      Register Value = valueRegister(Call.getArgOperand(2), MRI, MBB, TII);
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_ASSIGN_FIELD), Result)
          .addReg(Object)
          .addImm(Name)
          .addReg(Value);
      return;
    }

    if (Callee && cd::isIndexIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateIndexCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      Register Collection =
          valueRegister(Call.getArgOperand(0), MRI, MBB, TII);
      Register Index = valueRegister(Call.getArgOperand(1), MRI, MBB, TII);
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_INDEX), Result)
          .addReg(Collection)
          .addReg(Index);
      return;
    }

    if (Callee && cd::isAssignIndexIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateAssignIndexCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      Register Collection =
          valueRegister(Call.getArgOperand(0), MRI, MBB, TII);
      Register Index = valueRegister(Call.getArgOperand(1), MRI, MBB, TII);
      Register Value = valueRegister(Call.getArgOperand(2), MRI, MBB, TII);
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_ASSIGN_INDEX), Result)
          .addReg(Collection)
          .addReg(Index)
          .addReg(Value);
      return;
    }

    if (Callee && cd::isLenIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateLenCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      Register Value = valueRegister(Call.getArgOperand(0), MRI, MBB, TII);
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_LEN), Result)
          .addReg(Value);
      return;
    }

    if (Callee && cd::isAssertArrayIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateAssertArrayCall(Call, Error))
        unsupported(Error);

      Register Result = createValueRegister(MRI, &Call);
      Register Value = valueRegister(Call.getArgOperand(0), MRI, MBB, TII);
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_ASSERT_ARRAY), Result)
          .addReg(Value);
      return;
    }

    if (Callee && Callee->isDeclaration() &&
        (Callee->getName() == "cd_print" || Callee->getName() == "print") &&
        Call.arg_size() == 1 && Call.getType()->isVoidTy()) {
      if (!isSupportedPrintValue(Call.getArgOperand(0)))
        unsupported("a non-scalar print argument");
      Register Value = valueRegister(Call.getArgOperand(0), MRI, MBB, TII);
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_PRINT))
          .addReg(Value);
      return;
    }

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
        BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_CALL), Result)
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
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_LOAD_VAR), Result)
        .addImm(allocaName(Load.getPointerOperand()));
  }

  void lowerStore(const StoreInst &Store, MachineRegisterInfo &MRI,
                  MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (Store.isVolatile() || Store.isAtomic() ||
        !isSupportedValue(Store.getValueOperand()))
      unsupported("an unsupported store");

    Register Source =
        valueRegister(Store.getValueOperand(), MRI, MBB, TII);
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_STORE_VAR))
        .addImm(allocaName(Store.getPointerOperand()))
        .addReg(Source);
  }

  MachineBasicBlock *machineBlock(const BasicBlock *Block) const {
    auto It = MachineBlocks.find(Block);
    if (It == MachineBlocks.end())
      unsupported("a branch to an unknown basic block");
    return It->second;
  }

  void lowerPhiStores(const BasicBlock &Predecessor,
                      const BasicBlock &Successor, unsigned IncomingOccurrence,
                      MachineRegisterInfo &MRI, MachineBasicBlock &MBB,
                      const TargetInstrInfo &TII) {
    for (const PHINode &Phi : Successor.phis()) {
      int IncomingIndex = -1;
      unsigned Occurrence = 0;
      for (unsigned Index = 0; Index < Phi.getNumIncomingValues(); ++Index) {
        if (Phi.getIncomingBlock(Index) == &Predecessor) {
          if (Occurrence++ == IncomingOccurrence) {
            IncomingIndex = static_cast<int>(Index);
            break;
          }
        }
      }
      if (IncomingIndex < 0)
        unsupported("a PHI node without an incoming value for a branch edge");

      Register Source = valueRegister(
          Phi.getIncomingValue(static_cast<unsigned>(IncomingIndex)), MRI, MBB,
          TII);
      BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_STORE_VAR))
          .addImm(PhiNames.lookup(&Phi))
          .addReg(Source);
    }
  }

  void lowerUnconditionalBranch(const UncondBrInst &Branch,
                                const BasicBlock &Predecessor,
                                MachineRegisterInfo &MRI,
                                MachineBasicBlock &MBB,
                                const TargetInstrInfo &TII) {
    const BasicBlock *Successor = Branch.getSuccessor(0);
    lowerPhiStores(Predecessor, *Successor, 0, MRI, MBB, TII);
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_JUMP))
        .addMBB(machineBlock(Successor));
  }

  MachineBasicBlock *createConditionalEdge(
      MachineFunction &MF, const BasicBlock &Predecessor,
      const BasicBlock &Successor, unsigned IncomingOccurrence,
      MachineRegisterInfo &MRI, const TargetInstrInfo &TII) {
    MachineBasicBlock *Edge = MF.CreateMachineBasicBlock();
    MF.insert(MF.end(), Edge);
    lowerPhiStores(Predecessor, Successor, IncomingOccurrence, MRI, *Edge, TII);
    BuildMI(*Edge, Edge->end(), CurrentDebugLoc, TII.get(CD::CD_JUMP))
        .addMBB(machineBlock(&Successor));
    return Edge;
  }

  void lowerConditionalBranch(const CondBrInst &Branch,
                              const BasicBlock &Predecessor,
                              MachineFunction &MF, MachineRegisterInfo &MRI,
                              MachineBasicBlock &MBB,
                              const TargetInstrInfo &TII) {
    const BasicBlock *TrueSuccessor = Branch.getSuccessor(0);
    const BasicBlock *FalseSuccessor = Branch.getSuccessor(1);
    const unsigned FalseOccurrence = TrueSuccessor == FalseSuccessor ? 1 : 0;
    MachineBasicBlock *TrueEdge = machineBlock(TrueSuccessor);
    if (!TrueSuccessor->phis().empty())
      TrueEdge = createConditionalEdge(MF, Predecessor, *TrueSuccessor, 0, MRI,
                                       TII);
    MachineBasicBlock *FalseEdge = machineBlock(FalseSuccessor);
    if (!FalseSuccessor->phis().empty())
      FalseEdge = createConditionalEdge(MF, Predecessor, *FalseSuccessor,
                                        FalseOccurrence, MRI, TII);
    Register Condition =
        valueRegister(Branch.getCondition(), MRI, MBB, TII);
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_JUMP_IF_FALSE))
        .addReg(Condition)
        .addMBB(FalseEdge);
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_JUMP))
        .addMBB(TrueEdge);
  }

  void lowerSelect(const SelectInst &Select, MachineRegisterInfo &MRI,
                   MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (!Select.getCondition()->getType()->isIntegerTy(1) ||
        !isScalarType(Select.getType()) ||
        !isSupportedValue(Select.getTrueValue()) ||
        !isSupportedValue(Select.getFalseValue()))
      unsupported("an unsupported scalar select");

    Register Condition = valueRegister(Select.getCondition(), MRI, MBB, TII);
    Register TrueValue = valueRegister(Select.getTrueValue(), MRI, MBB, TII);
    Register FalseValue = valueRegister(Select.getFalseValue(), MRI, MBB, TII);
    Register Result = createValueRegister(MRI, &Select);
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_SELECT), Result)
        .addReg(Condition)
        .addReg(TrueValue)
        .addReg(FalseValue);
  }

  void lowerFNeg(const Instruction &Instruction, MachineRegisterInfo &MRI,
                 MachineBasicBlock &MBB, const TargetInstrInfo &TII) {
    if (!isScalarType(Instruction.getType()))
      unsupported("a non-scalar floating-point negation");

    Register Source =
        valueRegister(Instruction.getOperand(0), MRI, MBB, TII);
    Register Result = createValueRegister(MRI, &Instruction);
    BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(CD::CD_NEGATE), Result)
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
        BuildMI(MBB, MBB.end(), CurrentDebugLoc, TII.get(comparisonOpcode(Compare)),
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
    AllocaNames.clear();
    PhiNames.clear();
    MachineBlocks.clear();
    ParameterNames.clear();
    UsedStorageNames.clear();
    AllocaSerial = 0;
    ValueSerial = 0;
    CurrentDebugLoc = DebugLoc();

    if (IsMain && F.getName() != "main")
      unsupported("a non-main function as the entry body");
    if (F.isDeclaration())
      unsupported("a declared function body");
    if (IsMain && F.arg_size() != 0)
      unsupported("@main parameters in the first machine slice");
    if (F.empty())
      unsupported("a function without basic blocks");

    MachineFunction &MF = MMI.getOrCreateMachineFunction(F);
    if (MF.size() != 0)
      unsupported("pre-existing machine basic blocks");

    for (BasicBlock &BB : F) {
      MachineBasicBlock *MBB = MF.CreateMachineBasicBlock(&BB);
      MF.insert(MF.end(), MBB);
      MachineBlocks[&BB] = MBB;
    }
    MachineBasicBlock *EntryMBB = MachineBlocks.lookup(&F.getEntryBlock());
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
      BuildMI(*EntryMBB, EntryMBB->end(), DebugLoc(),
              TII.get(CD::CD_LOAD_VAR), Result)
          .addImm(addName(Name));
    }

    prepareStorage(F);
    preallocateValueRegisters(F, MRI);

    for (BasicBlock &BB : F) {
      MachineBasicBlock *MBB = MachineBlocks.lookup(&BB);
      for (const PHINode &Phi : BB.phis())
        BuildMI(*MBB, MBB->end(), Phi.getDebugLoc(), TII.get(CD::CD_LOAD_VAR),
                createValueRegister(MRI, &Phi))
            .addImm(PhiNames.lookup(&Phi));

      for (const Instruction &Instruction : BB) {
        CurrentDebugLoc = Instruction.getDebugLoc();
        if (isa<DbgInfoIntrinsic>(&Instruction) ||
            isa<PHINode>(&Instruction))
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
          BuildMI(*MBB, MBB->end(), CurrentDebugLoc, TII.get(CD::CD_RETURN))
              .addReg(Result);
          continue;
        }

        if (const auto *Branch = dyn_cast<CondBrInst>(&Instruction)) {
          lowerConditionalBranch(*Branch, BB, MF, MRI, *MBB, TII);
          continue;
        }

        if (const auto *Branch = dyn_cast<UncondBrInst>(&Instruction)) {
          lowerUnconditionalBranch(*Branch, BB, MRI, *MBB, TII);
          continue;
        }

        if (const auto *Select = dyn_cast<SelectInst>(&Instruction)) {
          lowerSelect(*Select, MRI, *MBB, TII);
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
    }

    CDBody Body;
    DenseMap<unsigned, unsigned> Registers;
    DenseMap<const MachineBasicBlock *, unsigned> BlockOffsets;
    struct BranchPatch {
      size_t Instruction;
      const MachineBasicBlock *Target;
    };
    std::vector<BranchPatch> BranchPatches;
    for (const MachineBasicBlock &Block : MF) {
      BlockOffsets[&Block] = Body.instructions.size();
      for (const MachineInstr &MI : Block) {
        if (MI.isDebugInstr())
          continue;

        const size_t PreviousInstructionCount = Body.instructions.size();
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
        case CD::CD_ARRAY: {
          if (MI.getNumOperands() < 1 || !MI.getOperand(0).isReg())
            unsupported("an invalid CD_ARRAY machine instruction");
          std::vector<unsigned> Elements;
          for (unsigned Index = 1; Index < MI.getNumOperands(); ++Index) {
            if (!MI.getOperand(Index).isReg())
              unsupported("an invalid CD_ARRAY element operand");
            Elements.push_back(artifactRegister(
                MI.getOperand(Index).getReg(), Registers, Body));
          }
          Body.instructions.push_back(CDInstruction::array(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              std::move(Elements)));
          break;
        }
        case CD::CD_MAP: {
          if (MI.getNumOperands() < 1 || !MI.getOperand(0).isReg() ||
              (MI.getNumOperands() - 1) % 2 != 0)
            unsupported("an invalid CD_MAP machine instruction");
          std::vector<unsigned> KeyValueOperands;
          for (unsigned Index = 1; Index < MI.getNumOperands(); ++Index) {
            if (!MI.getOperand(Index).isReg())
              unsupported("an invalid CD_MAP operand");
            KeyValueOperands.push_back(artifactRegister(
                MI.getOperand(Index).getReg(), Registers, Body));
          }
          Body.instructions.push_back(CDInstruction::map(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              std::move(KeyValueOperands)));
          break;
        }
        case CD::CD_STRUCT: {
          if (MI.getNumOperands() < 2 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isImm())
            unsupported("an invalid CD_STRUCT machine instruction");
          const int64_t TypeName = MI.getOperand(1).getImm();
          if (TypeName < -1)
            unsupported("an invalid CD_STRUCT type-name operand");
          if ((MI.getNumOperands() - 2) % 2 != 0)
            unsupported("an invalid CD_STRUCT field operand list");
          std::vector<unsigned> FieldNameValueOperands;
          for (unsigned Index = 2; Index < MI.getNumOperands(); Index += 2) {
            if (!MI.getOperand(Index).isImm() ||
                !MI.getOperand(Index + 1).isReg())
              unsupported("an invalid CD_STRUCT field operand");
            FieldNameValueOperands.push_back(
                static_cast<unsigned>(MI.getOperand(Index).getImm()));
            FieldNameValueOperands.push_back(artifactRegister(
                MI.getOperand(Index + 1).getReg(), Registers, Body));
          }
          Body.instructions.push_back(CDInstruction::structValue(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              TypeName < 0 ? cd::InvalidIndex
                           : static_cast<unsigned>(TypeName),
              std::move(FieldNameValueOperands)));
          break;
        }
        case CD::CD_VARIANT: {
          if (MI.getNumOperands() < 3 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isImm() || !MI.getOperand(2).isImm())
            unsupported("an invalid CD_VARIANT machine instruction");
          const int64_t EnumName = MI.getOperand(1).getImm();
          const int64_t VariantName = MI.getOperand(2).getImm();
          if (EnumName < 0 || VariantName < 0)
            unsupported("an invalid CD_VARIANT name operand");
          std::vector<unsigned> Payload;
          for (unsigned Index = 3; Index < MI.getNumOperands(); ++Index) {
            if (!MI.getOperand(Index).isReg())
              unsupported("an invalid CD_VARIANT payload operand");
            Payload.push_back(artifactRegister(
                MI.getOperand(Index).getReg(), Registers, Body));
          }
          Body.instructions.push_back(CDInstruction::variant(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              static_cast<unsigned>(EnumName),
              static_cast<unsigned>(VariantName), std::move(Payload)));
          break;
        }
        case CD::CD_VARIANT_TAG:
          if (MI.getNumOperands() != 4 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg() || !MI.getOperand(2).isImm() ||
              !MI.getOperand(3).isImm())
            unsupported("an invalid CD_VARIANT_TAG machine instruction");
          if (MI.getOperand(2).getImm() < 0 ||
              MI.getOperand(3).getImm() < 0)
            unsupported("an invalid CD_VARIANT_TAG name operand");
          Body.instructions.push_back(CDInstruction::variantTag(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body),
              static_cast<unsigned>(MI.getOperand(2).getImm()),
              static_cast<unsigned>(MI.getOperand(3).getImm())));
          break;
        case CD::CD_VARIANT_FIELD:
          if (MI.getNumOperands() != 3 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg() || !MI.getOperand(2).isImm())
            unsupported("an invalid CD_VARIANT_FIELD machine instruction");
          if (MI.getOperand(2).getImm() < 0)
            unsupported("an invalid CD_VARIANT_FIELD index operand");
          Body.instructions.push_back(CDInstruction::variantField(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body),
              static_cast<unsigned>(MI.getOperand(2).getImm())));
          break;
        case CD::CD_FIELD:
          if (MI.getNumOperands() != 3 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg() || !MI.getOperand(2).isImm())
            unsupported("an invalid CD_FIELD machine instruction");
          Body.instructions.push_back(CDInstruction::field(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body),
              static_cast<unsigned>(MI.getOperand(2).getImm())));
          break;
        case CD::CD_ASSIGN_FIELD:
          if (MI.getNumOperands() != 4 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg() || !MI.getOperand(2).isImm() ||
              !MI.getOperand(3).isReg())
            unsupported("an invalid CD_ASSIGN_FIELD machine instruction");
          Body.instructions.push_back(CDInstruction::assignField(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body),
              static_cast<unsigned>(MI.getOperand(2).getImm()),
              artifactRegister(MI.getOperand(3).getReg(), Registers, Body)));
          break;
        case CD::CD_INDEX:
          if (MI.getNumOperands() != 3 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg())
            unsupported("an invalid CD_INDEX machine instruction");
          Body.instructions.push_back(CDInstruction::index(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(2).getReg(), Registers, Body)));
          break;
        case CD::CD_ASSIGN_INDEX:
          if (MI.getNumOperands() != 4 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg() ||
              !MI.getOperand(3).isReg())
            unsupported("an invalid CD_ASSIGN_INDEX machine instruction");
          Body.instructions.push_back(CDInstruction::assignIndex(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(2).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(3).getReg(), Registers, Body)));
          break;
        case CD::CD_LEN:
        case CD::CD_ASSERT_ARRAY:
          if (MI.getNumOperands() != 2 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg())
            unsupported("an invalid unary collection machine instruction");
          Body.instructions.push_back(CDInstruction::unary(
              MI.getOpcode() == CD::CD_LEN ? CDOpcode::Len
                                           : CDOpcode::AssertArray,
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body)));
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
        case CD::CD_NATIVE_CALL: {
          if (MI.getNumOperands() < 2 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isImm())
            unsupported("an invalid CD_NATIVE_CALL machine instruction");
          const int64_t Name = MI.getOperand(1).getImm();
          if (Name < 0)
            unsupported("an invalid CD_NATIVE_CALL name operand");
          std::vector<unsigned> Arguments;
          for (unsigned Index = 2; Index < MI.getNumOperands(); ++Index) {
            if (!MI.getOperand(Index).isReg())
              unsupported("an invalid CD_NATIVE_CALL argument operand");
            Arguments.push_back(artifactRegister(
                MI.getOperand(Index).getReg(), Registers, Body));
          }
          Body.instructions.push_back(CDInstruction::nativeCall(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              static_cast<unsigned>(Name), std::move(Arguments)));
          break;
        }
        case CD::CD_PRINT:
          if (MI.getNumOperands() != 1 || !MI.getOperand(0).isReg())
            unsupported("an invalid CD_PRINT machine instruction");
          Body.instructions.push_back(CDInstruction::print(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body)));
          break;
        case CD::CD_SELECT: {
          if (MI.getNumOperands() != 4 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg() ||
              !MI.getOperand(3).isReg())
            unsupported("an invalid CD_SELECT machine instruction");
          const unsigned Destination =
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body);
          const unsigned Condition =
              artifactRegister(MI.getOperand(1).getReg(), Registers, Body);
          const unsigned TrueValue =
              artifactRegister(MI.getOperand(2).getReg(), Registers, Body);
          const unsigned FalseValue =
              artifactRegister(MI.getOperand(3).getReg(), Registers, Body);
          const size_t Conditional = Body.instructions.size();
          Body.instructions.push_back(
              CDInstruction::jumpIfFalse(Condition, cd::InvalidIndex));
          Body.instructions.push_back(
              CDInstruction::move(Destination, TrueValue));
          const size_t Join = Body.instructions.size();
          Body.instructions.push_back(CDInstruction::jump(cd::InvalidIndex));
          const unsigned FalseOffset = Body.instructions.size();
          Body.instructions.push_back(
              CDInstruction::move(Destination, FalseValue));
          Body.instructions[Conditional].target = FalseOffset;
          Body.instructions[Join].target = Body.instructions.size();
          break;
        }
        case CD::CD_JUMP: {
          if (MI.getNumOperands() != 1 || !MI.getOperand(0).isMBB())
            unsupported("an invalid CD_JUMP machine instruction");
          const size_t Instruction = Body.instructions.size();
          Body.instructions.push_back(CDInstruction::jump(cd::InvalidIndex));
          BranchPatches.push_back({Instruction, MI.getOperand(0).getMBB()});
          break;
        }
        case CD::CD_JUMP_IF_FALSE: {
          if (MI.getNumOperands() != 2 || !MI.getOperand(0).isReg() ||
              !MI.getOperand(1).isMBB())
            unsupported("an invalid CD_JUMP_IF_FALSE machine instruction");
          const size_t Instruction = Body.instructions.size();
          Body.instructions.push_back(CDInstruction::jumpIfFalse(
              artifactRegister(MI.getOperand(0).getReg(), Registers, Body),
              cd::InvalidIndex));
          BranchPatches.push_back({Instruction, MI.getOperand(1).getMBB()});
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

        const size_t AddedInstructions =
            Body.instructions.size() - PreviousInstructionCount;
        if (!Artifact.debugSources.empty() && AddedInstructions != 0) {
          std::optional<cd::CDDebugLocation> Location;
          std::string Error;
          if (!cd::resolveCDDebugLocation(MI.getDebugLoc(),
                                          Artifact.debugSources, Location,
                                          Error))
            unsupported(std::string("debug location ") + Error);
          Body.locations.insert(Body.locations.end(), AddedInstructions,
                                Location);
        }
      }
    }
    for (const BranchPatch &Patch : BranchPatches) {
      auto It = BlockOffsets.find(Patch.Target);
      if (It == BlockOffsets.end())
        unsupported("a branch target outside the machine function");
      Body.instructions[Patch.Instruction].target = It->second;
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

    std::vector<cd::CDDebugSource> DebugSources;
    std::string DebugError;
    if (!cd::parseCDSources(M, DebugSources, DebugError))
      unsupported(DebugError);
    Artifact.debugSources = std::move(DebugSources);

    for (const GlobalVariable &Global : M.globals()) {
      if (Global.isDeclaration())
        continue;
      if (Global.use_empty())
        unsupported("unused global variables");
      for (const User *User : Global.users()) {
        const auto *Call = dyn_cast<CallBase>(User);
        const bool IsStringUse =
            Call && cd::isStringIntrinsic(*Call) && Call->arg_size() > 0 &&
            Call->getArgOperand(0) == &Global;
        const bool IsNameUse = Call && cd::isNameOperand(*Call, Global);
        if (!IsStringUse && !IsNameUse)
          unsupported("globals used outside CD string/name intrinsics");
        std::string Error;
        if (IsStringUse) {
          if (!cd::getStringConstant(*Call, Error))
            unsupported(Error);
        } else if (cd::isNativeIntrinsic(*Call)) {
          if (!cd::validateNativeCall(*Call, Error))
            unsupported(Error);
        } else if (!cd::getNameConstant(Global, Error)) {
          unsupported(Error);
        }
      }
    }

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
