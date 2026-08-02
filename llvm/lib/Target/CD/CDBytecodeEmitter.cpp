//===-- CDBytecodeEmitter.cpp - CD bytecode emitter ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDBytecodeEmitter.h"
#include "CDBytecodeFormat.h"
#include "CDDebugInfo.h"
#include "CDValueABI.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
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

static bool isSupportedOperand(const Value *Value) {
  return isScalarType(Value->getType()) || isa<ConstantPointerNull>(Value);
}

static bool isSupportedPrintValue(const Value *Value) {
  return isSupportedOperand(Value) || cd::isCDValue(*Value);
}

[[noreturn]] static void unsupportedInstruction(const Instruction &I) {
  report_fatal_error(Twine("CD target does not support LLVM instruction: ") +
                    I.getOpcodeName());
}

[[noreturn]] static void unsupportedOperation(StringRef Operation) {
  report_fatal_error(Twine("CD target does not support LLVM operation: ") +
                    Operation);
}

class CDFunctionEmitter;

class CDModuleEmitter {
  raw_ostream &OS;
  std::vector<CDConstant> Constants;
  std::vector<std::string> Names;
  StringMap<unsigned> NameIndexes;
  StringMap<unsigned> ConstantIndexes;
  DenseMap<const Function *, unsigned> FunctionIndexes;

public:
  explicit CDModuleEmitter(raw_ostream &OS) : OS(OS) {}

  unsigned addName(StringRef Name) {
    auto It = NameIndexes.find(Name);
    if (It != NameIndexes.end())
      return It->second;
    Names.push_back(Name.str());
    const unsigned Index = Names.size() - 1;
    NameIndexes[Names.back()] = Index;
    return Index;
  }

  unsigned addConstant(CDConstant::Kind Kind, StringRef Text = {}) {
    std::string Key;
    Key.push_back(static_cast<char>('0' + Kind));
    Key += Text;
    auto It = ConstantIndexes.find(Key);
    if (It != ConstantIndexes.end())
      return It->second;
    Constants.push_back({Kind, Text.str()});
    const unsigned Index = Constants.size() - 1;
    ConstantIndexes[Key] = Index;
    return Index;
  }

  std::optional<unsigned> functionIndex(const Function *F) const {
    auto It = FunctionIndexes.find(F);
    if (It == FunctionIndexes.end())
      return std::nullopt;
    return It->second;
  }

  void emit(Module &M);
};

class CDFunctionEmitter {
  struct BranchPatch {
    size_t Line;
    const BasicBlock *Target = nullptr;
  };

  CDModuleEmitter &Module;
  Function &F;
  bool IsMain;
  DenseMap<const Value *, unsigned> ValueRegisters;
  DenseMap<const AllocaInst *, unsigned> AllocaNames;
  DenseMap<const PHINode *, unsigned> PhiNames;
  DenseMap<const BasicBlock *, unsigned> BlockOffsets;
  std::vector<BranchPatch> BranchPatches;
  std::vector<CDInstruction> Instructions;
  std::vector<std::string> ParameterNames;
  std::vector<unsigned> ParameterNameIndexes;
  std::set<std::string> UsedStorageNames;
  unsigned NextRegister = 0;
  unsigned AllocaSerial = 0;
  unsigned ValueSerial = 0;

  unsigned allocateRegister() { return NextRegister++; }

  void appendInstruction(CDInstruction Instruction) {
    Instructions.push_back(std::move(Instruction));
  }

  std::string uniqueStorageName(StringRef Base) {
    std::string Candidate = Base.str();
    if (Candidate.empty())
      Candidate = "value" + std::to_string(ValueSerial++);

    if (!UsedStorageNames.insert(Candidate).second) {
      const std::string Original = Candidate;
      unsigned Suffix = 1;
      do {
        Candidate = Original + "#" + std::to_string(Suffix++);
      } while (!UsedStorageNames.insert(Candidate).second);
    }
    return Candidate;
  }

  unsigned addStorageName(StringRef Base) {
    return Module.addName(uniqueStorageName(Base));
  }

  void validateFunctionTypes();
  void allocateValuesAndStorage();

  unsigned resultRegister(const Instruction &I) const {
    auto It = ValueRegisters.find(&I);
    if (It == ValueRegisters.end())
      unsupportedInstruction(I);
    return It->second;
  }

  unsigned materializeConstant(const Constant *C);
  unsigned materializeNil();
  unsigned valueRegister(const Value *V);

  unsigned nameRegister(const Value *V, StringRef Operation) {
    std::string Error;
    std::optional<StringRef> Name = cd::getNameConstant(*V, Error);
    if (!Name)
      unsupportedOperation((Operation + " " + Error).str());
    return Module.addName(*Name);
  }

  void emitBinary(const Instruction &I, CDOpcode Opcode) {
    const auto *BO = cast<BinaryOperator>(&I);
    if (!isScalarType(BO->getType()) || BO->getType()->isIntegerTy(1) ||
        !isSupportedOperand(BO->getOperand(0)) ||
        !isSupportedOperand(BO->getOperand(1)))
      unsupportedInstruction(I);

    appendInstruction(CDInstruction::binary(
        Opcode, resultRegister(I), valueRegister(BO->getOperand(0)),
        valueRegister(BO->getOperand(1))));
  }

  void emitCompare(const Instruction &I, CDOpcode Opcode) {
    const auto *Cmp = cast<CmpInst>(&I);
    if (!isSupportedOperand(Cmp->getOperand(0)) ||
        !isSupportedOperand(Cmp->getOperand(1)))
      unsupportedInstruction(I);

    appendInstruction(CDInstruction::binary(
        Opcode, resultRegister(I), valueRegister(Cmp->getOperand(0)),
        valueRegister(Cmp->getOperand(1))));
  }

  void emitCast(const Instruction &I, CDOpcode Opcode) {
    const auto *Cast = cast<CastInst>(&I);
    if (!isScalarType(Cast->getType()) ||
        !isSupportedOperand(Cast->getOperand(0)))
      unsupportedInstruction(I);

    appendInstruction(CDInstruction::unary(
        Opcode, resultRegister(I), valueRegister(Cast->getOperand(0))));
  }

  void emitUnary(const Instruction &I, CDOpcode Opcode) {
    if (!isScalarType(I.getType()) || !isSupportedOperand(I.getOperand(0)))
      unsupportedInstruction(I);

    appendInstruction(CDInstruction::unary(
        Opcode, resultRegister(I), valueRegister(I.getOperand(0))));
  }

  void emitNot(const Instruction &I) {
    const auto *BO = cast<BinaryOperator>(&I);
    if (!I.getType()->isIntegerTy(1))
      unsupportedInstruction(I);

    const Value *Input = nullptr;
    bool HasTrueConstant = false;
    for (const Use &Operand : BO->operands()) {
      if (const auto *Constant = dyn_cast<ConstantInt>(Operand.get())) {
        if (!Constant->getType()->isIntegerTy(1) || !Constant->isOne() ||
            HasTrueConstant)
          unsupportedInstruction(I);
        HasTrueConstant = true;
        continue;
      }

      if (Input || !Operand->getType()->isIntegerTy(1) ||
          !isSupportedOperand(Operand.get()))
        unsupportedInstruction(I);
      Input = Operand.get();
    }

    if (!Input || !HasTrueConstant)
      unsupportedInstruction(I);

    appendInstruction(CDInstruction::unary(
        CDOpcode::Not, resultRegister(I), valueRegister(Input)));
  }

  void emitSelect(const Instruction &I) {
    const auto *Select = cast<SelectInst>(&I);
    if (!Select->getCondition()->getType()->isIntegerTy(1) ||
        !isScalarType(Select->getType()) ||
        !isSupportedOperand(Select->getTrueValue()) ||
        !isSupportedOperand(Select->getFalseValue()))
      unsupportedInstruction(I);

    const unsigned Condition = valueRegister(Select->getCondition());
    const unsigned Destination = resultRegister(I);
    const size_t ConditionalLine = Instructions.size();
    appendInstruction(
        CDInstruction::jumpIfFalse(Condition, cd::InvalidIndex));
    appendInstruction(CDInstruction::move(
        Destination, valueRegister(Select->getTrueValue())));

    const size_t JoinJumpLine = Instructions.size();
    appendInstruction(CDInstruction::jump(cd::InvalidIndex));

    const unsigned FalseOffset = Instructions.size();
    Instructions[ConditionalLine].target = FalseOffset;
    appendInstruction(CDInstruction::move(
        Destination, valueRegister(Select->getFalseValue())));

    Instructions[JoinJumpLine].target = Instructions.size();
  }

  unsigned allocaName(const Value *Pointer) const {
    const auto *AI = dyn_cast<AllocaInst>(Pointer);
    if (!AI)
      unsupportedOperation("indirect load/store (only direct alloca values are supported)");
    auto It = AllocaNames.find(AI);
    if (It == AllocaNames.end())
      unsupportedOperation("unknown alloca value");
    return It->second;
  }

  void emitLoad(const LoadInst &Load) {
    if (Load.isVolatile() || Load.isAtomic() ||
        !isScalarType(Load.getType()))
      unsupportedInstruction(Load);

    appendInstruction(CDInstruction::loadVar(
        resultRegister(Load), allocaName(Load.getPointerOperand())));
  }

  void emitStore(const StoreInst &Store) {
    if (Store.isVolatile() || Store.isAtomic() ||
        !isSupportedOperand(Store.getValueOperand()))
      unsupportedInstruction(Store);

    appendInstruction(CDInstruction::storeVar(
        allocaName(Store.getPointerOperand()),
        valueRegister(Store.getValueOperand())));
  }

  void emitCall(const CallBase &Call) {
    Function *Callee = Call.getCalledFunction();
    if (!Callee)
      unsupportedInstruction(Call);

    if (cd::isStringIntrinsic(Call)) {
      std::string Error;
      std::optional<StringRef> Value = cd::getStringConstant(Call, Error);
      if (!Value)
        unsupportedOperation(Error);
      const unsigned Constant =
          Module.addConstant(CDConstant::String, *Value);
      appendInstruction(
          CDInstruction::constant(resultRegister(Call), Constant));
      return;
    }

    if (cd::isNativeIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateNativeCall(Call, Error))
        unsupportedOperation(Error);

      const unsigned Name =
          nameRegister(Call.getArgOperand(0), "llvm.cd.native");
      std::vector<unsigned> Arguments;
      Arguments.reserve(Call.arg_size() - 1);
      for (unsigned Index = 1; Index < Call.arg_size(); ++Index)
        Arguments.push_back(valueRegister(Call.getArgOperand(Index)));
      appendInstruction(CDInstruction::nativeCall(
          resultRegister(Call), Name, std::move(Arguments)));
      return;
    }

    if (cd::isArrayIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateArrayCall(Call, Error))
        unsupportedOperation(Error);

      std::vector<unsigned> Elements;
      Elements.reserve(Call.arg_size() - 1);
      for (unsigned Index = 1; Index < Call.arg_size(); ++Index)
        Elements.push_back(valueRegister(Call.getArgOperand(Index)));
      appendInstruction(
          CDInstruction::array(resultRegister(Call), std::move(Elements)));
      return;
    }

    if (cd::isMapIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateMapCall(Call, Error))
        unsupportedOperation(Error);

      std::vector<unsigned> KeyValueOperands;
      KeyValueOperands.reserve((Call.arg_size() - 1));
      for (unsigned Index = 1; Index < Call.arg_size(); ++Index)
        KeyValueOperands.push_back(valueRegister(Call.getArgOperand(Index)));
      appendInstruction(CDInstruction::map(resultRegister(Call),
                                            std::move(KeyValueOperands)));
      return;
    }

    if (cd::isStructIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateStructCall(Call, Error))
        unsupportedOperation(Error);

      unsigned TypeName = cd::InvalidIndex;
      if (!isa<ConstantPointerNull>(Call.getArgOperand(0)))
        TypeName = nameRegister(Call.getArgOperand(0), "llvm.cd.struct");

      std::vector<unsigned> FieldNameValueOperands;
      FieldNameValueOperands.reserve(Call.arg_size() - 2);
      for (unsigned Index = 2; Index < Call.arg_size(); Index += 2) {
        FieldNameValueOperands.push_back(
            nameRegister(Call.getArgOperand(Index), "llvm.cd.struct"));
        FieldNameValueOperands.push_back(
            valueRegister(Call.getArgOperand(Index + 1)));
      }
      appendInstruction(CDInstruction::structValue(
          resultRegister(Call), TypeName, std::move(FieldNameValueOperands)));
      return;
    }

    if (cd::isVariantIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateVariantCall(Call, Error))
        unsupportedOperation(Error);

      const unsigned EnumName =
          nameRegister(Call.getArgOperand(0), "llvm.cd.variant");
      const unsigned VariantName =
          nameRegister(Call.getArgOperand(1), "llvm.cd.variant");
      std::vector<unsigned> Payload;
      Payload.reserve(Call.arg_size() - 3);
      for (unsigned Index = 3; Index < Call.arg_size(); ++Index)
        Payload.push_back(valueRegister(Call.getArgOperand(Index)));
      appendInstruction(CDInstruction::variant(
          resultRegister(Call), EnumName, VariantName, std::move(Payload)));
      return;
    }

    if (cd::isVariantTagIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateVariantTagCall(Call, Error))
        unsupportedOperation(Error);
      appendInstruction(CDInstruction::variantTag(
          resultRegister(Call), valueRegister(Call.getArgOperand(0)),
          nameRegister(Call.getArgOperand(1), "llvm.cd.variant.tag"),
          nameRegister(Call.getArgOperand(2), "llvm.cd.variant.tag")));
      return;
    }

    if (cd::isVariantFieldIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateVariantFieldCall(Call, Error))
        unsupportedOperation(Error);
      const auto *Index = cast<ConstantInt>(Call.getArgOperand(1));
      appendInstruction(CDInstruction::variantField(
          resultRegister(Call), valueRegister(Call.getArgOperand(0)),
          static_cast<unsigned>(Index->getZExtValue())));
      return;
    }

    if (cd::isFieldIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateFieldCall(Call, Error))
        unsupportedOperation(Error);
      appendInstruction(CDInstruction::field(
          resultRegister(Call), valueRegister(Call.getArgOperand(0)),
          nameRegister(Call.getArgOperand(1), "llvm.cd.field")));
      return;
    }

    if (cd::isAssignFieldIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateAssignFieldCall(Call, Error))
        unsupportedOperation(Error);
      appendInstruction(CDInstruction::assignField(
          resultRegister(Call), valueRegister(Call.getArgOperand(0)),
          nameRegister(Call.getArgOperand(1), "llvm.cd.assign.field"),
          valueRegister(Call.getArgOperand(2))));
      return;
    }

    if (cd::isIndexIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateIndexCall(Call, Error))
        unsupportedOperation(Error);
      appendInstruction(CDInstruction::index(
          resultRegister(Call), valueRegister(Call.getArgOperand(0)),
          valueRegister(Call.getArgOperand(1))));
      return;
    }

    if (cd::isAssignIndexIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateAssignIndexCall(Call, Error))
        unsupportedOperation(Error);
      appendInstruction(CDInstruction::assignIndex(
          resultRegister(Call), valueRegister(Call.getArgOperand(0)),
          valueRegister(Call.getArgOperand(1)),
          valueRegister(Call.getArgOperand(2))));
      return;
    }

    if (cd::isLenIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateLenCall(Call, Error))
        unsupportedOperation(Error);
      appendInstruction(CDInstruction::len(
          resultRegister(Call), valueRegister(Call.getArgOperand(0))));
      return;
    }

    if (cd::isAssertArrayIntrinsic(Call)) {
      std::string Error;
      if (!cd::validateAssertArrayCall(Call, Error))
        unsupportedOperation(Error);
      appendInstruction(CDInstruction::assertArray(
          resultRegister(Call), valueRegister(Call.getArgOperand(0))));
      return;
    }

    if (Callee->isDeclaration() && Callee->getName() == "cd_print" &&
        Call.arg_size() == 1 && Call.getType()->isVoidTy()) {
      if (!isSupportedPrintValue(Call.getArgOperand(0)))
        unsupportedInstruction(Call);
      appendInstruction(CDInstruction::print(
          valueRegister(Call.getArgOperand(0))));
      return;
    }

    if (Callee->isDeclaration() && Callee->getName() == "print" &&
        Call.arg_size() == 1 && Call.getType()->isVoidTy()) {
      if (!isSupportedPrintValue(Call.getArgOperand(0)))
        unsupportedInstruction(Call);
      appendInstruction(CDInstruction::print(
          valueRegister(Call.getArgOperand(0))));
      return;
    }

    if (Callee->isIntrinsic())
      unsupportedInstruction(Call);

    auto FunctionIndex = Module.functionIndex(Callee);
    if (!FunctionIndex)
      unsupportedOperation("calls to declarations and the @main entry function");

    for (const Use &Argument : Call.args()) {
      if (!isSupportedOperand(Argument.get()))
        unsupportedInstruction(Call);
    }

    const unsigned FunctionRegister = allocateRegister();
    appendInstruction(
        CDInstruction::makeFunction(FunctionRegister, *FunctionIndex));

    const unsigned Destination = Call.getType()->isVoidTy()
                                     ? allocateRegister()
                                     : resultRegister(Call);
    std::vector<unsigned> Arguments;
    for (const Use &Argument : Call.args())
      Arguments.push_back(valueRegister(Argument.get()));
    appendInstruction(
        CDInstruction::call(Destination, FunctionRegister, std::move(Arguments)));
  }

  void emitPhiStores(const BasicBlock &Predecessor,
                     const BasicBlock &Successor, unsigned IncomingOccurrence) {
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
        unsupportedOperation("PHI node without an incoming value for a branch edge");

      appendInstruction(CDInstruction::storeVar(
          PhiNames.lookup(&Phi),
          valueRegister(Phi.getIncomingValue(
              static_cast<unsigned>(IncomingIndex)))));
    }
  }

  size_t appendJump(const BasicBlock *Target) {
    const size_t Line = Instructions.size();
    appendInstruction(CDInstruction::jump(cd::InvalidIndex));
    BranchPatches.push_back({Line, Target});
    return Line;
  }

  void emitTerminator(const BasicBlock &BB, const Instruction &Terminator);
  void emitInstruction(const Instruction &I);
  void emitBody();
  void patchBranches();

public:
  CDFunctionEmitter(CDModuleEmitter &Module, Function &F, bool IsMain)
      : Module(Module), F(F), IsMain(IsMain) {}

  CDBody emit();
};

void CDModuleEmitter::emit(Module &M) {
  Function *Main = M.getFunction("main");
  if (!Main || Main->isDeclaration())
    report_fatal_error("CD target requires a defined @main entry function");
  if (Main->arg_size() != 0)
    report_fatal_error("CD target @main must not have parameters");

  std::vector<cd::CDDebugSource> DebugSources;
  std::string DebugError;
  if (!cd::parseCDSources(M, DebugSources, DebugError))
    unsupportedOperation(DebugError);

  for (const GlobalVariable &Global : M.globals()) {
    if (Global.isDeclaration())
      continue;
    if (Global.use_empty())
      report_fatal_error("CD target does not support unused global variables");
      for (const User *User : Global.users()) {
        const auto *Call = dyn_cast<CallBase>(User);
      const bool IsStringUse =
          Call && cd::isStringIntrinsic(*Call) && Call->arg_size() > 0 &&
          Call->getArgOperand(0) == &Global;
      const bool IsNameUse = Call && cd::isNameOperand(*Call, Global);
      if (!IsStringUse && !IsNameUse)
        report_fatal_error(
            "CD target only supports globals used by CD string/name intrinsics");
      std::string Error;
      if (IsStringUse) {
        if (!cd::getStringConstant(*Call, Error))
          unsupportedOperation(Error);
      } else if (cd::isNativeIntrinsic(*Call)) {
        if (!cd::validateNativeCall(*Call, Error))
          unsupportedOperation(Error);
      } else if (!cd::getNameConstant(Global, Error)) {
        unsupportedOperation(Error);
      }
    }
  }

  unsigned FunctionIndex = 0;
  for (Function &F : M)
    if (&F != Main && !F.isDeclaration() && !F.isIntrinsic())
      FunctionIndexes[&F] = FunctionIndex++;

  CDBody MainBody = CDFunctionEmitter(*this, *Main, true).emit();
  std::vector<std::pair<Function *, CDBody>> FunctionBodies;
  for (Function &F : M)
    if (functionIndex(&F))
      FunctionBodies.emplace_back(&F,
                                  CDFunctionEmitter(*this, F, false).emit());

  cd::CDArtifact Artifact;
  Artifact.constants = std::move(Constants);
  Artifact.names = std::move(Names);
  Artifact.main = std::move(MainBody);
  Artifact.debugSources = std::move(DebugSources);
  for (unsigned Index = 0; Index < FunctionBodies.size(); ++Index) {
    const Function &F = *FunctionBodies[Index].first;
    const CDBody &Body = FunctionBodies[Index].second;
    Artifact.functions.push_back(
        {F.getName().str(), static_cast<unsigned>(Body.parameterNames.size()),
         std::move(FunctionBodies[Index].second)});
  }
  cd::serializeArtifact(Artifact, OS);
}

void CDFunctionEmitter::validateFunctionTypes() {
  for (const Argument &Argument : F.args())
    if (!isScalarType(Argument.getType()))
      unsupportedOperation("non-scalar function parameters");

  if (!F.getReturnType()->isVoidTy() && !isScalarType(F.getReturnType())) {
    if (!F.getReturnType()->isPointerTy())
      unsupportedOperation("non-scalar function return values");
    for (const BasicBlock &BB : F) {
      const auto *Return = dyn_cast<ReturnInst>(BB.getTerminator());
      if (!Return || !isa<ConstantPointerNull>(Return->getReturnValue()))
        unsupportedOperation("non-nil pointer function return values");
    }
  }
}

void CDFunctionEmitter::allocateValuesAndStorage() {
  validateFunctionTypes();

  for (Argument &Argument : F.args()) {
    const std::string Base = Argument.hasName()
                                 ? Argument.getName().str()
                                 : "arg" + std::to_string(ValueSerial++);
    const std::string StorageName = uniqueStorageName(Base);
    const unsigned NameIndex = Module.addName(StorageName);
    ValueRegisters[&Argument] = allocateRegister();
    if (!IsMain) {
      // Parameter metadata is consumed by the Rust VM as a local variable
      // binding.  The corresponding load is emitted in emitBody().
      ParameterNames.push_back(StorageName);
      ParameterNameIndexes.push_back(NameIndex);
    }
  }

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (isa<DbgInfoIntrinsic>(&I))
        continue;

      if (auto *AI = dyn_cast<AllocaInst>(&I)) {
        const auto *ArraySize = dyn_cast<ConstantInt>(AI->getArraySize());
        if (!ArraySize || !ArraySize->isOne() ||
            !isScalarType(AI->getAllocatedType()))
          unsupportedInstruction(I);
        const std::string Base = AI->hasName()
                                     ? AI->getName().str()
                                     : "alloca" + std::to_string(AllocaSerial++);
        AllocaNames[AI] = addStorageName(Base);
        continue;
      }

      if (auto *Phi = dyn_cast<PHINode>(&I)) {
        if (!isScalarType(Phi->getType()))
          unsupportedInstruction(I);
        const std::string Base = Phi->hasName()
                                     ? Phi->getName().str()
                                     : "value" + std::to_string(ValueSerial++);
        PhiNames[Phi] = addStorageName(Base);
      }

      if (!I.getType()->isVoidTy()) {
        if (!isScalarType(I.getType()) && !cd::isCDValue(I))
          unsupportedInstruction(I);
        ValueRegisters[&I] = allocateRegister();
      }
    }
  }
}

unsigned CDFunctionEmitter::materializeConstant(const Constant *C) {
  CDConstant::Kind Kind;
  std::string Text;
  if (const auto *CI = dyn_cast<ConstantInt>(C)) {
    if (CI->getType()->isIntegerTy(1)) {
      Kind = CDConstant::Bool;
      Text = CI->isOne() ? "true" : "false";
    } else {
      APFloat Converted(APFloat::IEEEdouble());
      const APFloat::opStatus Status = Converted.convertFromAPInt(
          CI->getValue(), /*IsSigned=*/true, APFloat::rmNearestTiesToEven);
      if (Status != APFloat::opOK)
        report_fatal_error(
            "CD target integer constant is not exactly representable as a number");
      const double Number = Converted.convertToDouble();
      Kind = CDConstant::Number;
      Text = cd::CDConstant::number(Number).text;
    }
  } else if (const auto *CFP = dyn_cast<ConstantFP>(C)) {
    const double Number = CFP->getValueAPF().convertToDouble();
    if (!std::isfinite(Number))
      report_fatal_error("CD target floating-point constant is not finite");
    Kind = CDConstant::Number;
    Text = cd::CDConstant::number(Number).text;
  } else if (isa<ConstantPointerNull>(C)) {
    Kind = CDConstant::Nil;
  } else {
    unsupportedOperation("aggregate, undef, poison, or expression constants");
  }

  const unsigned ConstantIndex = Module.addConstant(Kind, Text);
  const unsigned Register = allocateRegister();
  appendInstruction(CDInstruction::constant(Register, ConstantIndex));
  return Register;
}

unsigned CDFunctionEmitter::materializeNil() {
  const unsigned ConstantIndex = Module.addConstant(CDConstant::Nil);
  const unsigned Register = allocateRegister();
  appendInstruction(CDInstruction::constant(Register, ConstantIndex));
  return Register;
}

unsigned CDFunctionEmitter::valueRegister(const Value *V) {
  if (const auto *C = dyn_cast<Constant>(V))
    return materializeConstant(C);

  auto It = ValueRegisters.find(V);
  if (It != ValueRegisters.end())
    return It->second;

  if (isa<AllocaInst>(V))
    unsupportedOperation("using an alloca pointer as a bytecode value");
  unsupportedOperation("non-scalar or unassigned SSA values");
}

void CDFunctionEmitter::emitInstruction(const Instruction &I) {
  if (isa<DbgInfoIntrinsic>(&I) || isa<PHINode>(&I))
    return;

  switch (I.getOpcode()) {
  case Instruction::Alloca:
    return;
  case Instruction::Add:
  case Instruction::FAdd:
    emitBinary(I, CDOpcode::Add);
    return;
  case Instruction::Sub:
  case Instruction::FSub:
    emitBinary(I, CDOpcode::Subtract);
    return;
  case Instruction::Mul:
  case Instruction::FMul:
    emitBinary(I, CDOpcode::Multiply);
    return;
  case Instruction::FNeg:
    emitUnary(I, CDOpcode::Negate);
    return;
  case Instruction::Xor:
    emitNot(I);
    return;
  case Instruction::Select:
    emitSelect(I);
    return;
  case Instruction::SDiv:
  case Instruction::FDiv:
    emitBinary(I, CDOpcode::Divide);
    return;
  case Instruction::UDiv:
    unsupportedInstruction(I);
  case Instruction::ICmp: {
    const auto Predicate = cast<ICmpInst>(&I)->getPredicate();
    switch (Predicate) {
    case ICmpInst::ICMP_EQ:
      emitCompare(I, CDOpcode::Equal);
      return;
    case ICmpInst::ICMP_NE:
      emitCompare(I, CDOpcode::NotEqual);
      return;
    case ICmpInst::ICMP_SGT:
      emitCompare(I, CDOpcode::Greater);
      return;
    case ICmpInst::ICMP_SGE:
      emitCompare(I, CDOpcode::GreaterEqual);
      return;
    case ICmpInst::ICMP_SLT:
      emitCompare(I, CDOpcode::Less);
      return;
    case ICmpInst::ICMP_SLE:
      emitCompare(I, CDOpcode::LessEqual);
      return;
    case ICmpInst::ICMP_UGT:
    case ICmpInst::ICMP_UGE:
    case ICmpInst::ICMP_ULT:
    case ICmpInst::ICMP_ULE:
      unsupportedOperation("unsigned integer comparison predicate");
    default:
      unsupportedInstruction(I);
    }
  }
  case Instruction::FCmp: {
    const auto Predicate = cast<FCmpInst>(&I)->getPredicate();
    switch (Predicate) {
    case FCmpInst::FCMP_OEQ:
    case FCmpInst::FCMP_UEQ:
      emitCompare(I, CDOpcode::Equal);
      return;
    case FCmpInst::FCMP_ONE:
    case FCmpInst::FCMP_UNE:
      emitCompare(I, CDOpcode::NotEqual);
      return;
    case FCmpInst::FCMP_OGT:
    case FCmpInst::FCMP_UGT:
      emitCompare(I, CDOpcode::Greater);
      return;
    case FCmpInst::FCMP_OGE:
    case FCmpInst::FCMP_UGE:
      emitCompare(I, CDOpcode::GreaterEqual);
      return;
    case FCmpInst::FCMP_OLT:
    case FCmpInst::FCMP_ULT:
      emitCompare(I, CDOpcode::Less);
      return;
    case FCmpInst::FCMP_OLE:
    case FCmpInst::FCMP_ULE:
      emitCompare(I, CDOpcode::LessEqual);
      return;
    default:
      unsupportedInstruction(I);
    }
  }
  case Instruction::Trunc:
  case Instruction::ZExt:
  case Instruction::SExt:
  case Instruction::FPTrunc:
  case Instruction::FPExt:
  case Instruction::UIToFP:
  case Instruction::SIToFP:
  case Instruction::FPToUI:
  case Instruction::FPToSI:
  case Instruction::BitCast:
    emitCast(I, CDOpcode::Move);
    return;
  case Instruction::Load:
    emitLoad(cast<LoadInst>(I));
    return;
  case Instruction::Store:
    emitStore(cast<StoreInst>(I));
    return;
  case Instruction::Call:
    emitCall(cast<CallBase>(I));
    return;
  default:
    unsupportedInstruction(I);
  }
}

void CDFunctionEmitter::emitTerminator(const BasicBlock &BB,
                                       const Instruction &Terminator) {
  if (const auto *Return = dyn_cast<ReturnInst>(&Terminator)) {
    const unsigned Register = Return->getReturnValue()
                                  ? valueRegister(Return->getReturnValue())
                                  : materializeNil();
    appendInstruction(CDInstruction::returnValue(Register));
    return;
  }

  if (const auto *Branch = dyn_cast<UncondBrInst>(&Terminator)) {
    const BasicBlock *Successor = Branch->getSuccessor(0);
    emitPhiStores(BB, *Successor, 0);
    appendJump(Successor);
    return;
  }

  if (const auto *Branch = dyn_cast<CondBrInst>(&Terminator)) {
    const unsigned Condition = valueRegister(Branch->getCondition());
    const BasicBlock *TrueSuccessor = Branch->getSuccessor(0);
    const BasicBlock *FalseSuccessor = Branch->getSuccessor(1);
    const size_t ConditionalLine = Instructions.size();
    appendInstruction(
        CDInstruction::jumpIfFalse(Condition, cd::InvalidIndex));

    emitPhiStores(BB, *TrueSuccessor, 0);
    appendJump(TrueSuccessor);

    const unsigned FalseEdge = Instructions.size();
    Instructions[ConditionalLine].target = FalseEdge;
    const unsigned FalseOccurrence = TrueSuccessor == FalseSuccessor ? 1 : 0;
    emitPhiStores(BB, *FalseSuccessor, FalseOccurrence);
    appendJump(FalseSuccessor);
    return;
  }

  if (isa<UnreachableInst>(&Terminator))
    unsupportedInstruction(Terminator);
  unsupportedInstruction(Terminator);
}

void CDFunctionEmitter::emitBody() {
  for (unsigned Index = 0; Index < F.arg_size(); ++Index) {
    appendInstruction(CDInstruction::loadVar(
        ValueRegisters.lookup(F.getArg(Index)), ParameterNameIndexes[Index]));
  }

  for (BasicBlock &BB : F) {
    BlockOffsets[&BB] = Instructions.size();
    for (const PHINode &Phi : BB.phis()) {
      appendInstruction(CDInstruction::loadVar(resultRegister(Phi),
                                               PhiNames.lookup(&Phi)));
    }

    const Instruction *Terminator = BB.getTerminator();
    if (!Terminator)
      unsupportedOperation("basic blocks without terminators");
    for (const Instruction &I : BB) {
      if (&I == Terminator)
        break;
      emitInstruction(I);
    }
    emitTerminator(BB, *Terminator);
  }
}

void CDFunctionEmitter::patchBranches() {
  for (const BranchPatch &Patch : BranchPatches) {
    auto It = BlockOffsets.find(Patch.Target);
    if (It == BlockOffsets.end())
      unsupportedOperation("branch target outside the emitted function");
    const unsigned Target = It->second;
    Instructions[Patch.Line].target = Target;
  }
}

CDBody CDFunctionEmitter::emit() {
  allocateValuesAndStorage();
  emitBody();
  patchBranches();

  CDBody Body;
  Body.registers = NextRegister;
  Body.instructions = std::move(Instructions);
  Body.parameterNames = std::move(ParameterNames);
  return Body;
}

class CDBytecodeEmitter final : public ModulePass {
  raw_ostream &OS;

public:
  static char ID;

  explicit CDBytecodeEmitter(raw_ostream &OS) : ModulePass(ID), OS(OS) {}

  bool runOnModule(Module &M) override {
    CDModuleEmitter(OS).emit(M);
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override { AU.setPreservesAll(); }
};

char CDBytecodeEmitter::ID = 0;

} // namespace

ModulePass *llvm::createCDBytecodeEmitterPass(raw_ostream &OS) {
  return new CDBytecodeEmitter(OS);
}
