//===-- CDValueABI.cpp - CD LLVM value ABI helpers -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDValueABI.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/JSON.h"

using namespace llvm;

namespace llvm::cd {

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

bool isIndexIntrinsic(const CallBase &Call) {
  const Function *Callee = Call.getCalledFunction();
  return Callee && Callee->isIntrinsic() &&
         Callee->getIntrinsicID() == Intrinsic::cd_index;
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

bool isCDValue(const Value &Value) {
  if (const auto *Null = dyn_cast<ConstantPointerNull>(&Value))
    return cast<PointerType>(Null->getType())->getAddressSpace() == 0;

  const auto *Call = dyn_cast<CallBase>(&Value);
  return Call && (isStringIntrinsic(*Call) || isArrayIntrinsic(*Call) ||
                  isIndexIntrinsic(*Call) || isAssertArrayIntrinsic(*Call));
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
