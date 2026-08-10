//===-- CDValueABI.cpp - CD LLVM value ABI helpers -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDValueABI.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/JSON.h"

#include <limits>

using namespace llvm;

namespace llvm::cd {

static constexpr char CDValueParamsAttribute[] = "cd.value.params";
static constexpr char CDValueReturnAttribute[] = "cd.value.return";

static bool isScalarType(const Type *Type) {
  return Type->isIntegerTy() || Type->isFloatingPointTy();
}

static bool isAddressSpaceZeroPointer(const Type *Type) {
  return Type->isPointerTy() &&
         cast<PointerType>(Type)->getAddressSpace() == 0;
}

static bool parseCDValueParameterAttribute(
    const Function &F, SmallVectorImpl<unsigned> &Indices, std::string &Error) {
  if (!F.hasFnAttribute(CDValueParamsAttribute))
    return true;

  const Attribute Attribute = F.getFnAttribute(CDValueParamsAttribute);
  if (!Attribute.isStringAttribute()) {
    Error = "cd.value.params must be a string attribute";
    return false;
  }

  const StringRef Text = Attribute.getValueAsString();
  if (Text.empty()) {
    Error = "cd.value.params requires a non-empty parameter list";
    return false;
  }

  SmallVector<StringRef, 8> Parts;
  Text.split(Parts, ',', -1, /*KeepEmpty=*/true);
  unsigned Previous = 0;
  bool HasPrevious = false;
  for (StringRef Part : Parts) {
    unsigned Index = 0;
    if (Part.empty() || Part.getAsInteger(10, Index)) {
      Error = "cd.value.params requires decimal parameter indexes";
      return false;
    }
    if (HasPrevious && Index <= Previous) {
      Error = "cd.value.params indexes must be strictly increasing";
      return false;
    }
    if (Index >= F.arg_size()) {
      Error = "cd.value.params parameter index is out of range";
      return false;
    }

    const Type *ParameterType = F.getArg(Index)->getType();
    if (!ParameterType->isPointerTy()) {
      Error = "cd.value.params may mark only pointer parameters";
      return false;
    }
    if (!isAddressSpaceZeroPointer(ParameterType)) {
      Error = "cd.value.params requires address-space-zero pointer parameters";
      return false;
    }

    Indices.push_back(Index);
    Previous = Index;
    HasPrevious = true;
  }
  return true;
}

static bool isCDValueParameter(const Argument &Argument) {
  SmallVector<unsigned, 8> Indices;
  std::string Error;
  if (!parseCDValueParameterAttribute(*Argument.getParent(), Indices, Error))
    return false;
  for (unsigned Index : Indices)
    if (Index == Argument.getArgNo())
      return true;
  return false;
}

bool isStringIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_string;
}

bool isArrayIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_array;
}

bool isMapIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_map;
}

bool isStructIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_struct;
}

bool isVariantIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_variant;
}

bool isVariantTagIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_variant_tag;
}

bool isVariantFieldIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_variant_field;
}

bool isNativeIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_native;
}

bool isFieldIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_field;
}

bool isAssignFieldIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_assign_field;
}

bool isIndexIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_index;
}

bool isAssignIndexIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_assign_index;
}

bool isLenIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_len;
}

bool isAssertArrayIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_assert_array;
}

bool isCDNil(const Value &Value) {
  const auto *Null = dyn_cast<ConstantPointerNull>(&Value);
  return Null && isAddressSpaceZeroPointer(Null->getType());
}

bool isCDStorageAlloca(const AllocaInst &Alloca) {
  const auto *ArraySize = dyn_cast<ConstantInt>(Alloca.getArraySize());
  if (!ArraySize || !ArraySize->isOne() ||
      !isAddressSpaceZeroPointer(Alloca.getAllocatedType()))
    return false;

  for (const User *User : Alloca.users()) {
    if (isa<DbgInfoIntrinsic>(User))
      continue;
    if (const auto *Load = dyn_cast<LoadInst>(User)) {
      if (Load->getPointerOperand() != &Alloca ||
          !isAddressSpaceZeroPointer(Load->getType()))
        return false;
      continue;
    }
    if (const auto *Store = dyn_cast<StoreInst>(User)) {
      if (Store->getPointerOperand() != &Alloca)
        return false;
      continue;
    }
    return false;
  }
  return true;
}

static bool isCDValueImpl(
    const Value &Value, SmallPtrSetImpl<const llvm::Value *> &Visiting);

static bool isCDStorageLoad(
    const LoadInst &Load, SmallPtrSetImpl<const llvm::Value *> &Visiting);

static bool isCDValueImpl(
    const Value &Value, SmallPtrSetImpl<const llvm::Value *> &Visiting) {
  if (isCDNil(Value))
    return true;

  // Provenance is an all-incoming property. Treat a back-edge as provisionally
  // valid while still rejecting any unproven terminal value on that cycle.
  if (!Visiting.insert(&Value).second)
    return true;

  bool Result = false;

  if (const auto *Argument = dyn_cast<llvm::Argument>(&Value))
    Result = isCDValueParameter(*Argument);
  else if (const auto *Select = dyn_cast<SelectInst>(&Value))
    Result = Select->getCondition()->getType()->isIntegerTy(1) &&
             isAddressSpaceZeroPointer(Select->getType()) &&
             isCDValueImpl(*Select->getTrueValue(), Visiting) &&
             isCDValueImpl(*Select->getFalseValue(), Visiting);
  else if (const auto *Phi = dyn_cast<PHINode>(&Value)) {
    Result = isAddressSpaceZeroPointer(Phi->getType()) &&
             Phi->getNumIncomingValues() != 0;
    for (unsigned Index = 0; Result && Index < Phi->getNumIncomingValues();
         ++Index)
      Result = isCDValueImpl(*Phi->getIncomingValue(Index), Visiting);
  } else if (const auto *Load = dyn_cast<LoadInst>(&Value)) {
    Result = isCDStorageLoad(*Load, Visiting);
  } else if (const auto *Call = dyn_cast<CallBase>(&Value)) {
    if (const Function *Callee = Call->getCalledFunction())
      Result = !Callee->isDeclaration() && !Callee->isIntrinsic() &&
               Callee->hasFnAttribute(CDValueReturnAttribute) &&
               isAddressSpaceZeroPointer(Call->getType());

    if (!Result)
      Result = isStringIntrinsic(*Call) || isArrayIntrinsic(*Call) ||
               isMapIntrinsic(*Call) || isStructIntrinsic(*Call) ||
               (isVariantIntrinsic(*Call) &&
                isAddressSpaceZeroPointer(Call->getType())) ||
               (isVariantFieldIntrinsic(*Call) &&
                isAddressSpaceZeroPointer(Call->getType())) ||
               (isFieldIntrinsic(*Call) &&
                isAddressSpaceZeroPointer(Call->getType())) ||
               (isAssignFieldIntrinsic(*Call) &&
                isAddressSpaceZeroPointer(Call->getType())) ||
               isIndexIntrinsic(*Call) || isAssertArrayIntrinsic(*Call) ||
               (isAssignIndexIntrinsic(*Call) &&
                isAddressSpaceZeroPointer(Call->getType())) ||
               (isNativeIntrinsic(*Call) &&
                isAddressSpaceZeroPointer(Call->getType()));
  }

  Visiting.erase(&Value);
  return Result;
}

static bool isCDStorageLoad(
    const LoadInst &Load, SmallPtrSetImpl<const llvm::Value *> &Visiting) {
  if (Load.isVolatile() || Load.isAtomic() ||
      !isAddressSpaceZeroPointer(Load.getType()))
    return false;

  const auto *Alloca = dyn_cast<AllocaInst>(Load.getPointerOperand());
  if (!Alloca || !isCDStorageAlloca(*Alloca))
    return false;

  const Function *Parent = Alloca->getFunction();
  SmallVector<const StoreInst *, 8> Stores;
  for (const User *User : Alloca->users()) {
    if (isa<DbgInfoIntrinsic>(User))
      continue;
    if (const auto *Store = dyn_cast<StoreInst>(User)) {
      if (Store->isVolatile() || Store->isAtomic() ||
          !isCDValueImpl(*Store->getValueOperand(), Visiting))
        return false;
      Stores.push_back(Store);
    }
  }
  if (Stores.empty())
    return false;

  DenseMap<const BasicBlock *, bool> In;
  DenseMap<const BasicBlock *, bool> Out;
  for (const BasicBlock &BB : *Parent) {
    In[&BB] = false;
    Out[&BB] = false;
  }

  bool Changed = false;
  do {
    Changed = false;
    for (const BasicBlock &BB : *Parent) {
      bool NewIn = false;
      if (&BB != &Parent->getEntryBlock() && !pred_empty(&BB)) {
        NewIn = true;
        for (const BasicBlock *Pred : predecessors(&BB))
          NewIn = NewIn && Out.lookup(Pred);
      }

      bool NewOut = NewIn;
      for (const Instruction &Instruction : BB)
        if (const auto *Store = dyn_cast<StoreInst>(&Instruction))
          if (Store->getPointerOperand() == Alloca)
            NewOut = true;

      if (In.lookup(&BB) != NewIn || Out.lookup(&BB) != NewOut) {
        In[&BB] = NewIn;
        Out[&BB] = NewOut;
        Changed = true;
      }
    }
  } while (Changed);

  for (const BasicBlock &BB : *Parent) {
    bool Initialized = In.lookup(&BB);
    for (const Instruction &Instruction : BB) {
      if (const auto *LoadInBlock = dyn_cast<LoadInst>(&Instruction)) {
        if (LoadInBlock->getPointerOperand() == Alloca && !Initialized)
          return false;
      } else if (const auto *Store = dyn_cast<StoreInst>(&Instruction)) {
        if (Store->getPointerOperand() == Alloca)
          Initialized = true;
      }
    }
  }
  return true;
}

bool isCDValue(const Value &Value) {
  SmallPtrSet<const llvm::Value *, 16> Visiting;
  return isCDValueImpl(Value, Visiting);
}

bool validateFunctionABI(const Function &F, std::string &Error) {
  SmallVector<unsigned, 8> CDValueParameters;
  if (!parseCDValueParameterAttribute(F, CDValueParameters, Error))
    return false;

  for (unsigned Index = 0; Index < F.arg_size(); ++Index) {
    const Type *ParameterType = F.getArg(Index)->getType();
    bool IsCDValueParameter = false;
    for (unsigned MarkedIndex : CDValueParameters)
      if (MarkedIndex == Index) {
        IsCDValueParameter = true;
        break;
      }

    if (ParameterType->isPointerTy()) {
      if (!IsCDValueParameter) {
        Error = "every pointer function parameter must be listed by "
                "cd.value.params";
        return false;
      }
      if (!isAddressSpaceZeroPointer(ParameterType)) {
        Error = "cd.value.params requires address-space-zero pointer parameters";
        return false;
      }
      continue;
    }

    if (IsCDValueParameter) {
      Error = "cd.value.params may mark only pointer parameters";
      return false;
    }
    if (!isScalarType(ParameterType)) {
      Error = "non-scalar function parameters";
      return false;
    }
  }

  const bool HasCDValueReturn = F.hasFnAttribute(CDValueReturnAttribute);
  const Type *ReturnType = F.getReturnType();
  if (HasCDValueReturn) {
    if (!isAddressSpaceZeroPointer(ReturnType)) {
      Error = "cd.value.return requires an address-space-zero pointer return";
      return false;
    }
  } else if (ReturnType->isPointerTy()) {
    Error = "every pointer function return must carry cd.value.return";
    return false;
  } else if (!ReturnType->isVoidTy() && !isScalarType(ReturnType)) {
    Error = "non-scalar function return values";
    return false;
  }

  if (!ReturnType->isPointerTy() || F.isDeclaration())
    return true;

  for (const BasicBlock &BB : F) {
    const auto *Return = dyn_cast<ReturnInst>(BB.getTerminator());
    if (!Return)
      continue;
    if (!Return->getReturnValue() ||
        !isCDValue(*Return->getReturnValue())) {
      Error = "cd.value.return requires every pointer return value to have "
              "proven CD provenance";
      return false;
    }
  }
  return true;
}

static bool isNativeCallback(const Value &Value, bool ReturnsCDValue,
                             unsigned ParameterCount = 1) {
  const auto *Callback = dyn_cast<Function>(&Value);
  if (!Callback || Callback->isDeclaration() || Callback->isIntrinsic() ||
      Callback->getName() == "main")
    return false;

  std::string Error;
  SmallVector<unsigned, 8> Parameters;
  if (!validateFunctionABI(*Callback, Error) ||
      !parseCDValueParameterAttribute(*Callback, Parameters, Error) ||
      Parameters.size() != ParameterCount ||
      Callback->arg_size() != ParameterCount)
    return false;

  for (unsigned Index = 0; Index < ParameterCount; ++Index)
    if (Parameters[Index] != Index ||
        !isAddressSpaceZeroPointer(Callback->getArg(Index)->getType()))
      return false;

  if (ReturnsCDValue)
    return isAddressSpaceZeroPointer(Callback->getReturnType()) &&
           Callback->hasFnAttribute(CDValueReturnAttribute);
  return Callback->getReturnType()->isIntegerTy(1) &&
         !Callback->hasFnAttribute(CDValueReturnAttribute);
}

bool validateFunctionCall(const CallBase &Call, std::string &Error) {
  const Function *Callee = Call.getCalledFunction();
  if (!Callee || Callee->isDeclaration() || Callee->isIntrinsic()) {
    Error = "CD value transport requires a direct call to a defined function";
    return false;
  }
  if (!validateFunctionABI(*Callee, Error))
    return false;
  if (Call.arg_size() != Callee->arg_size()) {
    Error = "function call argument count does not match the callee ABI";
    return false;
  }

  for (unsigned Index = 0; Index < Call.arg_size(); ++Index) {
    const Value *Argument = Call.getArgOperand(Index);
    const Type *ParameterType = Callee->getArg(Index)->getType();
    if (Argument->getType() != ParameterType) {
      Error = "function call argument type does not match the callee ABI";
      return false;
    }
    if (ParameterType->isPointerTy()) {
      if (!isCDValue(*Argument)) {
        Error = "cd.value.params requires a proven CD value argument";
        return false;
      }
    } else if (!isScalarType(Argument->getType())) {
      Error = "function calls require scalar arguments or proven CD values";
      return false;
    }
  }
  return true;
}

bool isNameOperand(const CallBase &Call, const Value &Value) {
  if (isStructIntrinsic(Call)) {
    if (Call.arg_size() > 0 && Call.getArgOperand(0) == &Value)
      return true;
    for (unsigned Index = 2; Index < Call.arg_size(); Index += 2)
      if (Call.getArgOperand(Index) == &Value)
        return true;
  }
  if (isVariantIntrinsic(Call) && Call.arg_size() > 1 &&
      (Call.getArgOperand(0) == &Value || Call.getArgOperand(1) == &Value))
    return true;
  if (isVariantTagIntrinsic(Call) && Call.arg_size() > 2 &&
      (Call.getArgOperand(1) == &Value || Call.getArgOperand(2) == &Value))
    return true;
  if (isNativeIntrinsic(Call) && Call.arg_size() > 0 &&
      Call.getArgOperand(0) == &Value)
    return true;
  if ((isFieldIntrinsic(Call) || isAssignFieldIntrinsic(Call)) &&
      Call.arg_size() > 1 && Call.getArgOperand(1) == &Value)
    return true;
  return false;
}

bool isArrayElement(const Value &Value) {
  if (isa<UndefValue>(&Value) || isa<PoisonValue>(&Value))
    return false;

  if (Value.getType()->isIntegerTy() || Value.getType()->isFloatingPointTy())
    return true;

  return isCDValue(Value);
}

bool validateArrayCall(const CallBase &Call, std::string &Error) {
  if (!isArrayIntrinsic(Call)) {
    Error = "not an llvm.cd.array call";
    return false;
  }

  if (!Call.getType()->isPointerTy() ||
      cast<PointerType>(Call.getType())->getAddressSpace() != 0) {
    Error = "llvm.cd.array requires a ptr result";
    return false;
  }

  if (Call.arg_size() == 0) {
    Error = "llvm.cd.array requires an element-count immediate";
    return false;
  }

  const auto *Count = dyn_cast<ConstantInt>(Call.getArgOperand(0));
  if (!Count || !Count->getType()->isIntegerTy(32)) {
    Error = "llvm.cd.array requires an i32 element-count immediate";
    return false;
  }

  const uint64_t ElementCount = Count->getZExtValue();
  if (ElementCount != Call.arg_size() - 1) {
    Error = "llvm.cd.array element-count does not match the operand list";
    return false;
  }

  for (unsigned Index = 1; Index < Call.arg_size(); ++Index) {
    if (!isArrayElement(*Call.getArgOperand(Index))) {
      Error = "llvm.cd.array requires scalar, nil, string-token, or "
              "array-token operands";
      return false;
    }
  }

  return true;
}

static bool isMapKey(const Value &Value) {
  if (isa<UndefValue>(&Value) || isa<PoisonValue>(&Value))
    return false;
  if (Value.getType()->isIntegerTy() || Value.getType()->isFloatingPointTy())
    return true;
  if (const auto *Null = dyn_cast<ConstantPointerNull>(&Value))
    return cast<PointerType>(Null->getType())->getAddressSpace() == 0;
  const auto *Call = dyn_cast<CallBase>(&Value);
  return Call && isStringIntrinsic(*Call);
}

bool validateMapCall(const CallBase &Call, std::string &Error) {
  if (!isMapIntrinsic(Call)) {
    Error = "not an llvm.cd.map call";
    return false;
  }

  if (!Call.getType()->isPointerTy() ||
      cast<PointerType>(Call.getType())->getAddressSpace() != 0) {
    Error = "llvm.cd.map requires a ptr result";
    return false;
  }
  if (Call.arg_size() == 0) {
    Error = "llvm.cd.map requires an entry-count immediate";
    return false;
  }

  const auto *Count = dyn_cast<ConstantInt>(Call.getArgOperand(0));
  if (!Count || !Count->getType()->isIntegerTy(32)) {
    Error = "llvm.cd.map requires an i32 entry-count immediate";
    return false;
  }

  const uint64_t EntryCount = Count->getZExtValue();
  const uint64_t PayloadCount = Call.arg_size() - 1;
  if (EntryCount > std::numeric_limits<uint64_t>::max() / 2 ||
      EntryCount * 2 != PayloadCount) {
    Error = "llvm.cd.map entry-count does not match the key/value operand list";
    return false;
  }

  for (uint64_t Entry = 0; Entry < EntryCount; ++Entry) {
    const unsigned KeyIndex = 1 + static_cast<unsigned>(Entry * 2);
    const unsigned ValueIndex = KeyIndex + 1;
    if (!isMapKey(*Call.getArgOperand(KeyIndex))) {
      Error = "llvm.cd.map requires a scalar, nil, or string-token key "
              "operand";
      return false;
    }
    if (!isArrayElement(*Call.getArgOperand(ValueIndex))) {
      Error = "llvm.cd.map requires a scalar, nil, or CD dynamic-value "
              "value operand";
      return false;
    }
  }
  return true;
}

std::optional<StringRef> getNameConstant(const Value &Value,
                                          std::string &Error) {
  const auto *Global = dyn_cast<GlobalVariable>(&Value);
  if (!Global || Global->getAddressSpace() != 0 || !Global->isConstant() ||
      !Global->hasPrivateLinkage() || !Global->hasInitializer()) {
    Error = "requires a private non-empty string global name";
    return std::nullopt;
  }

  const Constant *Initializer = Global->getInitializer();
  const auto *Array = dyn_cast<ConstantDataArray>(Initializer);
  if (!Array || !Array->isString(8) || !Array->isCString()) {
    Error = "requires a private non-empty string global name";
    return std::nullopt;
  }

  StringRef Name = Array->getAsCString();
  if (Name.empty() || !json::isUTF8(Name)) {
    Error = "requires a private non-empty string global name";
    return std::nullopt;
  }
  return Name;
}

static bool validateNameOperand(const Value &Value, StringRef Operation,
                                StringRef Role, std::string &Error) {
  std::string NameError;
  if (getNameConstant(Value, NameError))
    return true;
  Error = (Operation + " requires a private non-empty string global " + Role)
              .str();
  return false;
}

static bool isNullCDPointer(const Value &Value) {
  const auto *Null = dyn_cast<ConstantPointerNull>(&Value);
  return Null && cast<PointerType>(Null->getType())->getAddressSpace() == 0;
}

static bool validateCDValueOperand(const CallBase &Call, unsigned Index,
                                   StringRef Operation, StringRef Role,
                                   std::string &Error);

static bool validateFieldResult(const CallBase &Call, StringRef Operation,
                                std::string &Error);

bool validateStructCall(const CallBase &Call, std::string &Error) {
  if (!isStructIntrinsic(Call)) {
    Error = "not an llvm.cd.struct call";
    return false;
  }
  if (!Call.getType()->isPointerTy() ||
      cast<PointerType>(Call.getType())->getAddressSpace() != 0) {
    Error = "llvm.cd.struct requires a ptr result";
    return false;
  }
  if (Call.arg_size() < 2) {
    Error = "llvm.cd.struct requires a type name and field-count operand";
    return false;
  }
  if (!isNullCDPointer(*Call.getArgOperand(0))) {
    if (!validateNameOperand(*Call.getArgOperand(0), "llvm.cd.struct",
                             "type name", Error)) {
      Error = "llvm.cd.struct requires an anonymous nil or private string "
              "global type name";
      return false;
    }
  }

  const auto *Count = dyn_cast<ConstantInt>(Call.getArgOperand(1));
  if (!Count || !Count->getType()->isIntegerTy(32)) {
    Error = "llvm.cd.struct requires an i32 field-count immediate";
    return false;
  }
  const uint64_t FieldCount = Count->getZExtValue();
  const uint64_t PayloadCount = Call.arg_size() - 2;
  if (FieldCount > std::numeric_limits<uint64_t>::max() / 2 ||
      FieldCount * 2 != PayloadCount) {
    Error = "llvm.cd.struct field-count does not match the field name/value "
            "operand list";
    return false;
  }

  for (uint64_t Field = 0; Field < FieldCount; ++Field) {
    const unsigned NameIndex = 2 + static_cast<unsigned>(Field * 2);
    const unsigned ValueIndex = NameIndex + 1;
    if (!validateNameOperand(*Call.getArgOperand(NameIndex), "llvm.cd.struct",
                             "field name", Error)) {
      Error = "llvm.cd.struct requires a private non-empty string global "
              "field name";
      return false;
    }
    if (!isArrayElement(*Call.getArgOperand(ValueIndex))) {
      Error = "llvm.cd.struct requires scalar, nil, or CD dynamic-value "
              "field operands";
      return false;
    }
  }
  return true;
}

bool validateVariantCall(const CallBase &Call, std::string &Error) {
  if (!isVariantIntrinsic(Call)) {
    Error = "not an llvm.cd.variant call";
    return false;
  }
  if (!Call.getType()->isPointerTy() ||
      cast<PointerType>(Call.getType())->getAddressSpace() != 0) {
    Error = "llvm.cd.variant requires a ptr result";
    return false;
  }
  if (Call.arg_size() < 3) {
    Error = "llvm.cd.variant requires enum name, variant name, and field-count operands";
    return false;
  }
  if (!validateNameOperand(*Call.getArgOperand(0), "llvm.cd.variant",
                           "enum name", Error)) {
    Error = "llvm.cd.variant requires private non-empty string global enum and variant names";
    return false;
  }
  if (!validateNameOperand(*Call.getArgOperand(1), "llvm.cd.variant",
                           "variant name", Error)) {
    Error = "llvm.cd.variant requires private non-empty string global enum and variant names";
    return false;
  }

  const auto *Count = dyn_cast<ConstantInt>(Call.getArgOperand(2));
  if (!Count || !Count->getType()->isIntegerTy(32)) {
    Error = "llvm.cd.variant requires an i32 field-count immediate";
    return false;
  }
  const uint64_t FieldCount = Count->getZExtValue();
  if (FieldCount != Call.arg_size() - 3) {
    Error = "llvm.cd.variant field-count does not match the payload operand list";
    return false;
  }
  for (unsigned Index = 3; Index < Call.arg_size(); ++Index) {
    if (!isArrayElement(*Call.getArgOperand(Index))) {
      Error = "llvm.cd.variant requires scalar, nil, or CD dynamic-value payload operands";
      return false;
    }
  }
  return true;
}

bool validateVariantTagCall(const CallBase &Call, std::string &Error) {
  if (!isVariantTagIntrinsic(Call)) {
    Error = "not an llvm.cd.variant.tag call";
    return false;
  }
  if (!Call.getType()->isIntegerTy(1)) {
    Error = "llvm.cd.variant.tag requires an i1 result";
    return false;
  }
  if (Call.arg_size() != 3) {
    Error = "llvm.cd.variant.tag requires a value, enum name, and variant name operand";
    return false;
  }
  if (!validateCDValueOperand(Call, 0, "llvm.cd.variant.tag", "value",
                              Error))
    return false;
  if (!validateNameOperand(*Call.getArgOperand(1), "llvm.cd.variant.tag",
                           "enum name", Error) ||
      !validateNameOperand(*Call.getArgOperand(2), "llvm.cd.variant.tag",
                           "variant name", Error)) {
    Error = "llvm.cd.variant.tag requires private non-empty string global enum and variant names";
    return false;
  }
  return true;
}

bool validateVariantFieldCall(const CallBase &Call, std::string &Error) {
  if (!isVariantFieldIntrinsic(Call)) {
    Error = "not an llvm.cd.variant.field call";
    return false;
  }
  if (!validateFieldResult(Call, "llvm.cd.variant.field", Error))
    return false;
  if (Call.arg_size() != 2) {
    Error = "llvm.cd.variant.field requires a value and field-index operand";
    return false;
  }
  if (!validateCDValueOperand(Call, 0, "llvm.cd.variant.field", "value",
                              Error))
    return false;
  const auto *Index = dyn_cast<ConstantInt>(Call.getArgOperand(1));
  if (!Index || !Index->getType()->isIntegerTy(32)) {
    Error = "llvm.cd.variant.field requires an i32 field-index immediate";
    return false;
  }
  return true;
}

bool validateNativeCall(const CallBase &Call, std::string &Error) {
  if (!isNativeIntrinsic(Call)) {
    Error = "not an llvm.cd.native call";
    return false;
  }
  if (Call.arg_size() == 0) {
    Error = "llvm.cd.native requires a native name operand";
    return false;
  }

  std::string NameError;
  std::optional<StringRef> Name =
      getNameConstant(*Call.getArgOperand(0), NameError);
  if (!Name) {
    Error = "llvm.cd.native requires a private non-empty string global native name";
    return false;
  }

  const StringRef NativeName = *Name;
  const bool HasDoubleResult = Call.getType()->isDoubleTy();
  const bool HasCDPointerResult =
      Call.getType()->isPointerTy() &&
      cast<PointerType>(Call.getType())->getAddressSpace() == 0;

  if (NativeName == "floor" || NativeName == "ceil" ||
      NativeName == "sqrt") {
    if (Call.arg_size() != 2 ||
        !Call.getArgOperand(1)->getType()->isDoubleTy() ||
        !HasDoubleResult) {
      Error = ("llvm.cd.native " + NativeName.str() +
               " requires one double argument and a double result");
      return false;
    }
    return true;
  }

  if (NativeName == "str" || NativeName == "typeOf") {
    if (Call.arg_size() != 2 || !isArrayElement(*Call.getArgOperand(1)) ||
        !HasCDPointerResult) {
      Error = ("llvm.cd.native " + NativeName.str() +
               " requires one scalar or CD dynamic-value argument and a ptr result");
      return false;
    }
    return true;
  }

  if (NativeName == "hash") {
    if (Call.arg_size() != 2 || !isArrayElement(*Call.getArgOperand(1)) ||
        !HasDoubleResult) {
      Error = "llvm.cd.native hash requires one scalar or CD dynamic-value "
              "argument and a double result";
      return false;
    }
    return true;
  }

  if (NativeName == "contains") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !isArrayElement(*Call.getArgOperand(2)) ||
        !Call.getType()->isIntegerTy(1)) {
      Error = "llvm.cd.native contains requires a CD dynamic-value "
              "collection, a scalar or CD dynamic-value needle, and an i1 "
              "result";
      return false;
    }
    return true;
  }

  if (NativeName == "slice") {
    if (Call.arg_size() != 4 || !isCDValue(*Call.getArgOperand(1)) ||
        !Call.getArgOperand(2)->getType()->isDoubleTy() ||
        !Call.getArgOperand(3)->getType()->isDoubleTy() ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native slice requires a CD dynamic-value array, two "
              "double arguments, and a ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "copy") {
    if (Call.arg_size() != 2 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native copy requires a CD dynamic-value array and a "
              "ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "concat") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !isCDValue(*Call.getArgOperand(2)) || !HasCDPointerResult) {
      Error = "llvm.cd.native concat requires two CD dynamic-value arrays "
              "and a ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "push") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !isArrayElement(*Call.getArgOperand(2)) || !HasCDPointerResult) {
      Error = "llvm.cd.native push requires a CD dynamic-value array, a "
              "scalar or CD dynamic-value value, and a ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "pop") {
    if (Call.arg_size() != 2 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native pop requires a CD dynamic-value array and a "
              "ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "remove") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !isArrayElement(*Call.getArgOperand(2)) || !HasCDPointerResult) {
      Error = "llvm.cd.native remove requires a CD dynamic-value map, a "
              "scalar or CD dynamic-value key, and a ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "clear") {
    if (Call.arg_size() != 2 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native clear requires a CD dynamic-value map and a "
              "ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "merge") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !isCDValue(*Call.getArgOperand(2)) || !HasCDPointerResult) {
      Error = "llvm.cd.native merge requires two CD dynamic-value maps and a "
              "ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "keys") {
    if (Call.arg_size() != 2 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native keys requires a CD dynamic-value map and a ptr "
              "result";
      return false;
    }
    return true;
  }

  if (NativeName == "values") {
    if (Call.arg_size() != 2 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native values requires a CD dynamic-value map and a "
              "ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "range") {
    if (Call.arg_size() < 2 || Call.arg_size() > 4 ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native range requires one to three double arguments "
              "and a ptr result";
      return false;
    }
    for (unsigned Index = 1; Index < Call.arg_size(); ++Index) {
      if (!Call.getArgOperand(Index)->getType()->isDoubleTy()) {
        Error = "llvm.cd.native range requires one to three double arguments "
                "and a ptr result";
        return false;
      }
    }
    return true;
  }

  if (NativeName == "substr") {
    if (Call.arg_size() != 4 || !isCDValue(*Call.getArgOperand(1)) ||
        !Call.getArgOperand(2)->getType()->isDoubleTy() ||
        !Call.getArgOperand(3)->getType()->isDoubleTy() ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native substr requires a CD string value, two double "
              "arguments, and a ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "charAt") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !Call.getArgOperand(2)->getType()->isDoubleTy() ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native charAt requires a CD string value, one double "
              "argument, and a ptr result";
      return false;
    }
    return true;
  }

  if (NativeName == "map" || NativeName == "flatMap") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasCDPointerResult) {
      Error = ("llvm.cd.native " + NativeName.str() +
               " requires a CD dynamic-value array, a direct callback, and "
               "a ptr result");
      return false;
    }

    if (!isNativeCallback(*Call.getArgOperand(2), true)) {
      Error = ("llvm.cd.native " + NativeName.str() +
               " requires a direct defined callback with one "
               "address-space-zero CD parameter and a cd.value.return pointer "
               "result");
      return false;
    }
    return true;
  }

  if (NativeName == "reduce") {
    if (Call.arg_size() != 4 || !isCDValue(*Call.getArgOperand(1)) ||
        !isArrayElement(*Call.getArgOperand(2)) || !HasCDPointerResult) {
      Error = "llvm.cd.native reduce requires a CD dynamic-value array, a "
              "scalar or CD dynamic-value initial value, a direct callback, "
              "and a ptr result";
      return false;
    }
    if (!isNativeCallback(*Call.getArgOperand(3), true, 2)) {
      Error = "llvm.cd.native reduce requires a direct defined callback with "
              "two address-space-zero CD parameters and a cd.value.return "
              "pointer result";
      return false;
    }
    return true;
  }

  if (NativeName == "filter") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native filter requires a CD dynamic-value array, a "
              "direct callback, and a ptr result";
      return false;
    }
    if (!isNativeCallback(*Call.getArgOperand(2), false)) {
      Error = "llvm.cd.native filter requires a direct defined callback with "
              "one address-space-zero CD parameter and an i1 result";
      return false;
    }
    return true;
  }

  if (NativeName == "any" || NativeName == "all") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !Call.getType()->isIntegerTy(1)) {
      Error = ("llvm.cd.native " + NativeName.str() +
               " requires a CD dynamic-value array, a direct callback, and "
               "an i1 result");
      return false;
    }
    if (!isNativeCallback(*Call.getArgOperand(2), false)) {
      Error = ("llvm.cd.native " + NativeName.str() +
               " requires a direct defined callback with one "
               "address-space-zero CD parameter and an i1 result");
      return false;
    }
    return true;
  }

  if (NativeName == "count") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasDoubleResult) {
      Error = "llvm.cd.native count requires a CD dynamic-value array, a "
              "direct callback, and a double result";
      return false;
    }
    if (!isNativeCallback(*Call.getArgOperand(2), false)) {
      Error = "llvm.cd.native count requires a direct defined callback with "
              "one address-space-zero CD parameter and an i1 result";
      return false;
    }
    return true;
  }

  if (NativeName == "find") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasCDPointerResult) {
      Error = "llvm.cd.native find requires a CD dynamic-value array, a "
              "direct callback, and a ptr result";
      return false;
    }
    if (!isNativeCallback(*Call.getArgOperand(2), false)) {
      Error = "llvm.cd.native find requires a direct defined callback with "
              "one address-space-zero CD parameter and an i1 result";
      return false;
    }
    return true;
  }

  if (NativeName == "findIndex") {
    if (Call.arg_size() != 3 || !isCDValue(*Call.getArgOperand(1)) ||
        !HasDoubleResult) {
      Error = "llvm.cd.native findIndex requires a CD dynamic-value array, a "
              "direct callback, and a double result";
      return false;
    }
    if (!isNativeCallback(*Call.getArgOperand(2), false)) {
      Error = "llvm.cd.native findIndex requires a direct defined callback "
              "with one address-space-zero CD parameter and an i1 result";
      return false;
    }
    return true;
  }

  Error = ("llvm.cd.native native name is not supported by the bounded CD "
           "ABI: " +
           NativeName.str());
  return false;
}

static bool validateFieldResult(const CallBase &Call, StringRef Operation,
                                std::string &Error) {
  if (Call.getType()->isIntegerTy() || Call.getType()->isFloatingPointTy())
    return true;
  if (Call.getType()->isPointerTy() &&
      cast<PointerType>(Call.getType())->getAddressSpace() == 0)
    return true;
  Error = (Operation + " requires a scalar or address-space-zero CD value "
                    "result")
              .str();
  return false;
}

bool validateFieldCall(const CallBase &Call, std::string &Error) {
  if (!isFieldIntrinsic(Call)) {
    Error = "not an llvm.cd.field call";
    return false;
  }
  if (!validateFieldResult(Call, "llvm.cd.field", Error))
    return false;
  if (Call.arg_size() != 2) {
    Error = "llvm.cd.field requires an object and field-name operand";
    return false;
  }
  if (!validateCDValueOperand(Call, 0, "llvm.cd.field", "object", Error))
    return false;
  return validateNameOperand(*Call.getArgOperand(1), "llvm.cd.field",
                             "field name", Error);
}

bool validateAssignFieldCall(const CallBase &Call, std::string &Error) {
  if (!isAssignFieldIntrinsic(Call)) {
    Error = "not an llvm.cd.assign.field call";
    return false;
  }
  if (Call.arg_size() != 3) {
    Error = "llvm.cd.assign.field requires an object, field-name, and value "
            "operand";
    return false;
  }
  if (!validateCDValueOperand(Call, 0, "llvm.cd.assign.field", "object",
                              Error))
    return false;
  if (!validateNameOperand(*Call.getArgOperand(1), "llvm.cd.assign.field",
                           "field name", Error))
    return false;
  const Value *Assigned = Call.getArgOperand(2);
  if (!isArrayElement(*Assigned)) {
    Error = "llvm.cd.assign.field requires scalar, nil, or CD dynamic-value "
            "assigned value";
    return false;
  }
  if (Call.getType() != Assigned->getType()) {
    Error = "llvm.cd.assign.field result type must match the assigned value "
            "type";
    return false;
  }
  if (!validateFieldResult(Call, "llvm.cd.assign.field", Error))
    return false;
  return true;
}

static bool validateCDValueOperand(const CallBase &Call, unsigned Index,
                                   StringRef Operation, StringRef Role,
                                   std::string &Error) {
  if (Index >= Call.arg_size() || !isCDValue(*Call.getArgOperand(Index))) {
    Error = (Operation + " requires a CD dynamic-value " + Role).str();
    return false;
  }
  return true;
}

bool validateIndexCall(const CallBase &Call, std::string &Error) {
  if (!isIndexIntrinsic(Call)) {
    Error = "not an llvm.cd.index call";
    return false;
  }

  if (!Call.getType()->isPointerTy() ||
      cast<PointerType>(Call.getType())->getAddressSpace() != 0) {
    Error = "llvm.cd.index requires a ptr result";
    return false;
  }
  if (Call.arg_size() != 2) {
    Error = "llvm.cd.index requires a collection and index operand";
    return false;
  }
  if (!validateCDValueOperand(Call, 0, "llvm.cd.index", "collection", Error))
    return false;
  if (!Call.getArgOperand(1)->getType()->isDoubleTy()) {
    Error = "llvm.cd.index requires a double index";
    return false;
  }
  return true;
}

bool validateAssignIndexCall(const CallBase &Call, std::string &Error) {
  if (!isAssignIndexIntrinsic(Call)) {
    Error = "not an llvm.cd.assign.index call";
    return false;
  }

  if (Call.arg_size() != 3) {
    Error = "llvm.cd.assign.index requires a collection, index, and "
            "value operand";
    return false;
  }
  if (!validateCDValueOperand(Call, 0, "llvm.cd.assign.index", "collection",
                              Error))
    return false;
  if (!Call.getArgOperand(1)->getType()->isDoubleTy()) {
    Error = "llvm.cd.assign.index requires a double index";
    return false;
  }

  const Value *Assigned = Call.getArgOperand(2);
  if (!isArrayElement(*Assigned)) {
    Error = "llvm.cd.assign.index requires a scalar, nil, or CD "
            "dynamic-value assigned value";
    return false;
  }
  if (Call.getType() != Assigned->getType()) {
    Error = "llvm.cd.assign.index result type must match the assigned value type";
    return false;
  }
  if (!Call.getType()->isIntegerTy() && !Call.getType()->isFloatingPointTy() &&
      !isCDValue(Call)) {
    Error = "llvm.cd.assign.index requires a scalar or address-space-zero "
            "CD value result";
    return false;
  }
  return true;
}

bool validateLenCall(const CallBase &Call, std::string &Error) {
  if (!isLenIntrinsic(Call)) {
    Error = "not an llvm.cd.len call";
    return false;
  }

  if (!Call.getType()->isDoubleTy()) {
    Error = "llvm.cd.len requires a double result";
    return false;
  }
  if (Call.arg_size() != 1) {
    Error = "llvm.cd.len requires one collection operand";
    return false;
  }
  return validateCDValueOperand(Call, 0, "llvm.cd.len", "operand", Error);
}

bool validateAssertArrayCall(const CallBase &Call, std::string &Error) {
  if (!isAssertArrayIntrinsic(Call)) {
    Error = "not an llvm.cd.assert.array call";
    return false;
  }

  if (!Call.getType()->isPointerTy() ||
      cast<PointerType>(Call.getType())->getAddressSpace() != 0) {
    Error = "llvm.cd.assert.array requires a ptr result";
    return false;
  }
  if (Call.arg_size() != 1) {
    Error = "llvm.cd.assert.array requires one operand";
    return false;
  }
  return validateCDValueOperand(Call, 0, "llvm.cd.assert.array", "operand",
                                Error);
}

std::optional<StringRef> getStringConstant(const CallBase &Call,
                                           std::string &Error) {
  if (!isStringIntrinsic(Call)) {
    Error = "not an llvm.cd.string call";
    return std::nullopt;
  }

  if (Call.arg_size() != 1 || !Call.getType()->isPointerTy()) {
    Error = "llvm.cd.string requires one ptr argument and a ptr result";
    return std::nullopt;
  }

  const auto *Global = dyn_cast<GlobalVariable>(Call.getArgOperand(0));
  if (!Global)
    Error = "llvm.cd.string requires a direct string global operand";
  else if (Global->getAddressSpace() != 0 || !Global->isConstant() ||
           !Global->hasPrivateLinkage())
    Error = "llvm.cd.string requires a private constant address-space-zero global";
  else if (!Global->hasInitializer())
    Error = "llvm.cd.string requires a string global initializer";
  else {
    const Constant *Initializer = Global->getInitializer();
    const auto *Array = dyn_cast<ConstantDataArray>(Initializer);
    const auto *ArrayTy = dyn_cast<ArrayType>(Global->getValueType());
    if (Initializer->isNullValue() && ArrayTy &&
        ArrayTy->getNumElements() == 1 &&
        ArrayTy->getElementType()->isIntegerTy(8))
      return StringRef();
    if (!Array || !Array->isString(8) || !Array->isCString())
      Error = "llvm.cd.string requires a nul-terminated byte string global";
    else {
      StringRef Value = Array->getAsCString();
      if (!json::isUTF8(Value))
        Error = "llvm.cd.string requires valid UTF-8 bytes";
      else
        return Value;
    }
  }

  return std::nullopt;
}

} // namespace llvm::cd
