//===-- CDModuleInfo.h - CD module metadata helpers -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDMODULEINFO_H
#define LLVM_LIB_TARGET_CD_CDMODULEINFO_H

#include "CDBytecodeFormat.h"

#include <string>

namespace llvm {
class Module;

namespace cd {

/// Return whether the module contains explicit CD module metadata.
bool hasCDModuleMetadata(const Module &M);

/// Parse the explicit !cd.module and optional !cd.dependencies contract.
/// The module node must contain exactly one record.
bool parseCDModuleMetadata(const Module &M, CDModuleMetadata &Metadata,
                           std::string &Error);

} // namespace cd
} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDMODULEINFO_H
