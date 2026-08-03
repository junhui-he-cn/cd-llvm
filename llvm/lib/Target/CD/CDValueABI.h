//===-- CDValueABI.h - CD LLVM value ABI helpers --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDVALUEABI_H
#define LLVM_LIB_TARGET_CD_CDVALUEABI_H

#include "llvm/ADT/StringRef.h"

#include <optional>
#include <string>

namespace llvm {
class CallBase;
class Function;
class Value;

namespace cd {

bool isStringIntrinsic(const CallBase &Call);

bool isArrayIntrinsic(const CallBase &Call);

bool isMapIntrinsic(const CallBase &Call);

bool isStructIntrinsic(const CallBase &Call);

bool isVariantIntrinsic(const CallBase &Call);

bool isVariantTagIntrinsic(const CallBase &Call);

bool isVariantFieldIntrinsic(const CallBase &Call);

bool isNativeIntrinsic(const CallBase &Call);

bool isFieldIntrinsic(const CallBase &Call);

bool isAssignFieldIntrinsic(const CallBase &Call);

bool isIndexIntrinsic(const CallBase &Call);

bool isAssignIndexIntrinsic(const CallBase &Call);

bool isLenIntrinsic(const CallBase &Call);

bool isAssertArrayIntrinsic(const CallBase &Call);

/// Return whether a pointer value is the address-space-zero CD nil token.
bool isCDNil(const Value &Value);

/// Return whether a value is an opaque CD dynamic-value token.
bool isCDValue(const Value &Value);

/// Validate the function attributes and pointer-value return boundary used by
/// the first dynamic-value transport slice.
bool validateFunctionABI(const Function &Function, std::string &Error);

/// Validate a direct call to a defined function using that function's CD ABI.
bool validateFunctionCall(const CallBase &Call, std::string &Error);

/// Return whether a global value is used as a CD struct/field name operand.
bool isNameOperand(const CallBase &Call, const Value &Value);

/// Return whether a value is a valid operand capability for llvm.cd.array.
bool isArrayElement(const Value &Value);

/// Validate the result, immediate element count, and variadic operands of
/// llvm.cd.array.
bool validateArrayCall(const CallBase &Call, std::string &Error);

bool validateMapCall(const CallBase &Call, std::string &Error);

bool validateStructCall(const CallBase &Call, std::string &Error);

bool validateVariantCall(const CallBase &Call, std::string &Error);

bool validateVariantTagCall(const CallBase &Call, std::string &Error);

bool validateVariantFieldCall(const CallBase &Call, std::string &Error);

bool validateNativeCall(const CallBase &Call, std::string &Error);

bool validateFieldCall(const CallBase &Call, std::string &Error);

bool validateAssignFieldCall(const CallBase &Call, std::string &Error);

bool validateIndexCall(const CallBase &Call, std::string &Error);

bool validateAssignIndexCall(const CallBase &Call, std::string &Error);

bool validateLenCall(const CallBase &Call, std::string &Error);

bool validateAssertArrayCall(const CallBase &Call, std::string &Error);

/// Extract the immutable UTF-8 payload accepted by llvm.cd.string.
///
/// The returned StringRef points into the module's immutable global
/// initializer and remains valid for the duration of lowering.
std::optional<StringRef> getStringConstant(const CallBase &Call,
                                           std::string &Error);

/// Extract a non-empty UTF-8 name from a private constant byte global.
std::optional<StringRef> getNameConstant(const Value &Value,
                                          std::string &Error);

} // namespace cd
} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDVALUEABI_H
