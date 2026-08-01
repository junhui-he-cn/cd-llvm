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
class Value;

namespace cd {

bool isStringIntrinsic(const CallBase &Call);

bool isArrayIntrinsic(const CallBase &Call);

bool isIndexIntrinsic(const CallBase &Call);

bool isLenIntrinsic(const CallBase &Call);

bool isAssertArrayIntrinsic(const CallBase &Call);

/// Return whether a value is an opaque CD dynamic-value token.
bool isCDValue(const Value &Value);

/// Return whether a value is a valid operand capability for llvm.cd.array.
bool isArrayElement(const Value &Value);

/// Validate the result, immediate element count, and variadic operands of
/// llvm.cd.array.
bool validateArrayCall(const CallBase &Call, std::string &Error);

bool validateIndexCall(const CallBase &Call, std::string &Error);

bool validateLenCall(const CallBase &Call, std::string &Error);

bool validateAssertArrayCall(const CallBase &Call, std::string &Error);

/// Extract the immutable UTF-8 payload accepted by llvm.cd.string.
///
/// The returned StringRef points into the module's immutable global
/// initializer and remains valid for the duration of lowering.
std::optional<StringRef> getStringConstant(const CallBase &Call,
                                           std::string &Error);

} // namespace cd
} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDVALUEABI_H
