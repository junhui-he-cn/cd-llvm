//===-- CDBytecodeEmitter.cpp - CD bytecode emitter ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDBytecodeEmitter.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

namespace {

struct CDConstant {
  enum Kind { Nil, Number, Bool };

  Kind Type;
  std::string Text;
};

struct CDBody {
  unsigned Registers = 0;
  std::vector<std::string> ParameterNames;
  std::vector<std::string> Instructions;
};

static std::string registerName(unsigned Register) {
  return "r" + std::to_string(Register);
}

static std::string constantName(unsigned Constant) {
  return "c" + std::to_string(Constant);
}

static std::string nameName(unsigned Name) { return "n" + std::to_string(Name); }

static std::string functionName(unsigned Function) {
  return "f" + std::to_string(Function);
}

static std::string escapeString(StringRef Value) {
  std::string Result;
  Result.reserve(Value.size() + 2);
  for (char Character : Value) {
    switch (Character) {
    case '\\':
      Result += "\\\\";
      break;
    case '"':
      Result += "\\\"";
      break;
    case '\n':
      Result += "\\n";
      break;
    case '\r':
      Result += "\\r";
      break;
    case '\t':
      Result += "\\t";
      break;
    default:
      Result += Character;
      break;
    }
  }
  return Result;
}

static void writeQuoted(raw_ostream &OS, StringRef Value) {
  OS << '"' << escapeString(Value) << '"';
}

static std::string numberText(double Number) {
  std::string Text;
  raw_string_ostream Stream(Text);
  Stream << format("%.17g", Number);
  Stream.flush();
  return Text;
}

static bool isScalarType(const Type *Type) {
  return Type->isIntegerTy() || Type->isFloatingPointTy();
}

static bool isSupportedOperand(const Value *Value) {
  return isScalarType(Value->getType()) || isa<ConstantPointerNull>(Value);
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
  DenseMap<const Function *, unsigned> FunctionIndexes;

public:
  explicit CDModuleEmitter(raw_ostream &OS) : OS(OS) {}

  unsigned addName(StringRef Name) {
    for (unsigned Index = 0; Index < Names.size(); ++Index)
      if (Names[Index] == Name)
        return Index;
    Names.push_back(Name.str());
    return Names.size() - 1;
  }

  unsigned addConstant(CDConstant::Kind Kind, StringRef Text = {}) {
    for (unsigned Index = 0; Index < Constants.size(); ++Index)
      if (Constants[Index].Type == Kind && Constants[Index].Text == Text)
        return Index;
    Constants.push_back({Kind, Text.str()});
    return Constants.size() - 1;
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
    enum Kind { Jump, JumpIfFalse, JumpIfTrue };

    size_t Line;
    Kind Opcode;
    unsigned Condition = 0;
    const BasicBlock *Target = nullptr;
  };

  CDModuleEmitter &Module;
  Function &F;
  bool IsMain;
  DenseMap<const Value *, unsigned> ValueRegisters;
  DenseMap<const AllocaInst *, unsigned> AllocaNames;
  DenseMap<const PHINode *, unsigned> PhiNames;
  DenseMap<const BasicBlock *, unsigned> BlockOffsets;
  DenseMap<const Constant *, unsigned> ConstantRegisters;
  std::vector<BranchPatch> BranchPatches;
  std::vector<std::string> Lines;
  std::vector<std::string> ParameterNames;
  std::vector<unsigned> ParameterNameIndexes;
  std::set<std::string> UsedStorageNames;
  unsigned NextRegister = 0;
  unsigned AllocaSerial = 0;
  unsigned ValueSerial = 0;

  unsigned allocateRegister() { return NextRegister++; }

  void appendLine(std::string Line) {
    Lines.push_back("  " + std::move(Line));
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

  void emitBinary(const Instruction &I, StringRef Opcode) {
    const auto *BO = cast<BinaryOperator>(&I);
    if (!isScalarType(BO->getType()) || BO->getType()->isIntegerTy(1) ||
        !isSupportedOperand(BO->getOperand(0)) ||
        !isSupportedOperand(BO->getOperand(1)))
      unsupportedInstruction(I);

    appendLine(registerName(resultRegister(I)) + " = " + Opcode.str() + " " +
                registerName(valueRegister(BO->getOperand(0))) + ", " +
                registerName(valueRegister(BO->getOperand(1))));
  }

  void emitCompare(const Instruction &I, StringRef Opcode) {
    const auto *Cmp = cast<CmpInst>(&I);
    if (!isSupportedOperand(Cmp->getOperand(0)) ||
        !isSupportedOperand(Cmp->getOperand(1)))
      unsupportedInstruction(I);

    appendLine(registerName(resultRegister(I)) + " = " + Opcode.str() + " " +
                registerName(valueRegister(Cmp->getOperand(0))) + ", " +
                registerName(valueRegister(Cmp->getOperand(1))));
  }

  void emitCast(const Instruction &I, StringRef OpcodeName) {
    const auto *Cast = cast<CastInst>(&I);
    if (!isScalarType(Cast->getType()) ||
        !isSupportedOperand(Cast->getOperand(0)))
      unsupportedInstruction(I);

    appendLine(registerName(resultRegister(I)) + " = " + OpcodeName.str() +
                " " + registerName(valueRegister(Cast->getOperand(0))));
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

    appendLine(registerName(resultRegister(Load)) + " = load_var " +
                nameName(allocaName(Load.getPointerOperand())));
  }

  void emitStore(const StoreInst &Store) {
    if (Store.isVolatile() || Store.isAtomic() ||
        !isSupportedOperand(Store.getValueOperand()))
      unsupportedInstruction(Store);

    appendLine("store_var " + nameName(allocaName(Store.getPointerOperand())) +
                ", " + registerName(valueRegister(Store.getValueOperand())));
  }

  void emitCall(const CallBase &Call) {
    Function *Callee = Call.getCalledFunction();
    if (!Callee)
      unsupportedInstruction(Call);

    if (Callee->isDeclaration() && Callee->getName() == "cd_print" &&
        Call.arg_size() == 1 && Call.getType()->isVoidTy()) {
      if (!isSupportedOperand(Call.getArgOperand(0)))
        unsupportedInstruction(Call);
      appendLine("print " + registerName(valueRegister(Call.getArgOperand(0))));
      return;
    }

    if (Callee->isDeclaration() && Callee->getName() == "print" &&
        Call.arg_size() == 1 && Call.getType()->isVoidTy()) {
      if (!isSupportedOperand(Call.getArgOperand(0)))
        unsupportedInstruction(Call);
      appendLine("print " + registerName(valueRegister(Call.getArgOperand(0))));
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
    appendLine(registerName(FunctionRegister) + " = make_function " +
                functionName(*FunctionIndex));

    const unsigned Destination = Call.getType()->isVoidTy()
                                     ? allocateRegister()
                                     : resultRegister(Call);
    std::string Line = registerName(Destination) + " = call " +
                       registerName(FunctionRegister) + " [";
    for (unsigned Index = 0; Index < Call.arg_size(); ++Index) {
      if (Index != 0)
        Line += ", ";
      Line += registerName(valueRegister(Call.getArgOperand(Index)));
    }
    Line += "]";
    appendLine(std::move(Line));
  }

  void emitPhiStores(const BasicBlock &Predecessor,
                     const BasicBlock &Successor) {
    for (const PHINode &Phi : Successor.phis()) {
      int IncomingIndex = -1;
      for (unsigned Index = 0; Index < Phi.getNumIncomingValues(); ++Index) {
        if (Phi.getIncomingBlock(Index) == &Predecessor) {
          IncomingIndex = static_cast<int>(Index);
          break;
        }
      }
      if (IncomingIndex < 0)
        unsupportedOperation("PHI node without an incoming value for a branch edge");

      appendLine("store_var " + nameName(PhiNames.lookup(&Phi)) + ", " +
                  registerName(valueRegister(
                      Phi.getIncomingValue(static_cast<unsigned>(IncomingIndex)))));
    }
  }

  size_t appendJump(BranchPatch::Kind Kind, const BasicBlock *Target,
                    unsigned Condition = 0) {
    const size_t Line = Lines.size();
    Lines.emplace_back();
    BranchPatches.push_back({Line, Kind, Condition, Target});
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

  for (const GlobalVariable &Global : M.globals())
    if (!Global.isDeclaration())
      report_fatal_error("CD target does not support global variables");

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

  OS << "cdbc 0.1\n\nconstants:\n";
  for (unsigned Index = 0; Index < Constants.size(); ++Index) {
    OS << "  " << constantName(Index) << " = ";
    switch (Constants[Index].Type) {
    case CDConstant::Nil:
      OS << "nil";
      break;
    case CDConstant::Number:
      OS << "number " << Constants[Index].Text;
      break;
    case CDConstant::Bool:
      OS << "bool " << Constants[Index].Text;
      break;
    }
    OS << '\n';
  }

  OS << "\nnames:\n";
  for (unsigned Index = 0; Index < Names.size(); ++Index) {
    OS << "  " << nameName(Index) << " = ";
    writeQuoted(OS, Names[Index]);
    OS << '\n';
  }

  OS << "\nmain registers=" << MainBody.Registers << ":\n";
  for (const std::string &Line : MainBody.Instructions)
    OS << Line << '\n';

  for (unsigned Index = 0; Index < FunctionBodies.size(); ++Index) {
    const Function &F = *FunctionBodies[Index].first;
    const CDBody &Body = FunctionBodies[Index].second;
    OS << "\nfunction " << functionName(Index) << " name=";
    writeQuoted(OS, F.getName());
    OS << " arity=" << Body.ParameterNames.size()
       << " registers=" << Body.Registers << ":\n";
    for (unsigned Parameter = 0; Parameter < Body.ParameterNames.size();
         ++Parameter) {
      OS << "  param " << Parameter << " = ";
      writeQuoted(OS, Body.ParameterNames[Parameter]);
      OS << '\n';
    }
    for (const std::string &Line : Body.Instructions)
      OS << Line << '\n';
  }
}

void CDFunctionEmitter::validateFunctionTypes() {
  for (const Argument &Argument : F.args())
    if (!isScalarType(Argument.getType()))
      unsupportedOperation("non-scalar function parameters");

  if (!F.getReturnType()->isVoidTy() && !isScalarType(F.getReturnType()))
    unsupportedOperation("non-scalar function return values");
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
        if (!isScalarType(I.getType()))
          unsupportedInstruction(I);
        ValueRegisters[&I] = allocateRegister();
      }
    }
  }
}

unsigned CDFunctionEmitter::materializeConstant(const Constant *C) {
  auto It = ConstantRegisters.find(C);
  if (It != ConstantRegisters.end())
    return It->second;

  CDConstant::Kind Kind;
  std::string Text;
  if (const auto *CI = dyn_cast<ConstantInt>(C)) {
    if (CI->getType()->isIntegerTy(1)) {
      Kind = CDConstant::Bool;
      Text = CI->isOne() ? "true" : "false";
    } else {
      const double Number = CI->getValue().signedRoundToDouble();
      if (!std::isfinite(Number))
        report_fatal_error("CD target integer constant is not representable as a finite number");
      Kind = CDConstant::Number;
      Text = numberText(Number);
    }
  } else if (const auto *CFP = dyn_cast<ConstantFP>(C)) {
    const double Number = CFP->getValueAPF().convertToDouble();
    if (!std::isfinite(Number))
      report_fatal_error("CD target floating-point constant is not finite");
    Kind = CDConstant::Number;
    Text = numberText(Number);
  } else if (isa<ConstantPointerNull>(C)) {
    Kind = CDConstant::Nil;
  } else {
    unsupportedOperation("aggregate, undef, poison, or expression constants");
  }

  const unsigned ConstantIndex = Module.addConstant(Kind, Text);
  const unsigned Register = allocateRegister();
  ConstantRegisters[C] = Register;
  appendLine(registerName(Register) + " = constant " +
              constantName(ConstantIndex));
  return Register;
}

unsigned CDFunctionEmitter::materializeNil() {
  const unsigned ConstantIndex = Module.addConstant(CDConstant::Nil);
  const unsigned Register = allocateRegister();
  appendLine(registerName(Register) + " = constant " +
              constantName(ConstantIndex));
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
    emitBinary(I, "add");
    return;
  case Instruction::Sub:
  case Instruction::FSub:
    emitBinary(I, "subtract");
    return;
  case Instruction::Mul:
  case Instruction::FMul:
    emitBinary(I, "multiply");
    return;
  case Instruction::SDiv:
  case Instruction::UDiv:
  case Instruction::FDiv:
    emitBinary(I, "divide");
    return;
  case Instruction::ICmp: {
    const auto Predicate = cast<ICmpInst>(&I)->getPredicate();
    switch (Predicate) {
    case ICmpInst::ICMP_EQ:
      emitCompare(I, "equal");
      return;
    case ICmpInst::ICMP_NE:
      emitCompare(I, "not_equal");
      return;
    case ICmpInst::ICMP_UGT:
    case ICmpInst::ICMP_SGT:
      emitCompare(I, "greater");
      return;
    case ICmpInst::ICMP_UGE:
    case ICmpInst::ICMP_SGE:
      emitCompare(I, "greater_equal");
      return;
    case ICmpInst::ICMP_ULT:
    case ICmpInst::ICMP_SLT:
      emitCompare(I, "less");
      return;
    case ICmpInst::ICMP_ULE:
    case ICmpInst::ICMP_SLE:
      emitCompare(I, "less_equal");
      return;
    default:
      unsupportedInstruction(I);
    }
  }
  case Instruction::FCmp: {
    const auto Predicate = cast<FCmpInst>(&I)->getPredicate();
    switch (Predicate) {
    case FCmpInst::FCMP_OEQ:
    case FCmpInst::FCMP_UEQ:
      emitCompare(I, "equal");
      return;
    case FCmpInst::FCMP_ONE:
    case FCmpInst::FCMP_UNE:
      emitCompare(I, "not_equal");
      return;
    case FCmpInst::FCMP_OGT:
    case FCmpInst::FCMP_UGT:
      emitCompare(I, "greater");
      return;
    case FCmpInst::FCMP_OGE:
    case FCmpInst::FCMP_UGE:
      emitCompare(I, "greater_equal");
      return;
    case FCmpInst::FCMP_OLT:
    case FCmpInst::FCMP_ULT:
      emitCompare(I, "less");
      return;
    case FCmpInst::FCMP_OLE:
    case FCmpInst::FCMP_ULE:
      emitCompare(I, "less_equal");
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
    emitCast(I, "move");
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
    appendLine("return " + registerName(Register));
    return;
  }

  if (const auto *Branch = dyn_cast<BranchInst>(&Terminator)) {
    if (Branch->isUnconditional()) {
      const BasicBlock *Successor = Branch->getSuccessor(0);
      emitPhiStores(BB, *Successor);
      appendJump(BranchPatch::Jump, Successor);
      return;
    }

    const unsigned Condition = valueRegister(Branch->getCondition());
    const BasicBlock *TrueSuccessor = Branch->getSuccessor(0);
    const BasicBlock *FalseSuccessor = Branch->getSuccessor(1);
    const size_t ConditionalLine = Lines.size();
    Lines.emplace_back();

    emitPhiStores(BB, *TrueSuccessor);
    appendJump(BranchPatch::Jump, TrueSuccessor);

    const unsigned FalseEdge = Lines.size();
    Lines[ConditionalLine] = "  jump_if_false " + registerName(Condition) +
                             ", " + std::to_string(FalseEdge);
    emitPhiStores(BB, *FalseSuccessor);
    appendJump(BranchPatch::Jump, FalseSuccessor);
    return;
  }

  if (isa<UnreachableInst>(&Terminator))
    unsupportedInstruction(Terminator);
  unsupportedInstruction(Terminator);
}

void CDFunctionEmitter::emitBody() {
  for (unsigned Index = 0; Index < F.arg_size(); ++Index)
    appendLine(registerName(ValueRegisters.lookup(F.getArg(Index))) +
                " = load_var " + nameName(ParameterNameIndexes[Index]));

  for (BasicBlock &BB : F) {
    BlockOffsets[&BB] = Lines.size();
    for (const PHINode &Phi : BB.phis())
      appendLine(registerName(resultRegister(Phi)) + " = load_var " +
                  nameName(PhiNames.lookup(&Phi)));

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
    switch (Patch.Opcode) {
    case BranchPatch::Jump:
      Lines[Patch.Line] = "  jump " + std::to_string(Target);
      break;
    case BranchPatch::JumpIfFalse:
      Lines[Patch.Line] = "  jump_if_false " +
                          registerName(Patch.Condition) + ", " +
                          std::to_string(Target);
      break;
    case BranchPatch::JumpIfTrue:
      Lines[Patch.Line] = "  jump_if_true " + registerName(Patch.Condition) +
                          ", " + std::to_string(Target);
      break;
    }
  }
}

CDBody CDFunctionEmitter::emit() {
  allocateValuesAndStorage();
  emitBody();
  patchBranches();

  CDBody Body;
  Body.Registers = NextRegister;
  Body.Instructions = std::move(Lines);
  Body.ParameterNames = std::move(ParameterNames);
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
