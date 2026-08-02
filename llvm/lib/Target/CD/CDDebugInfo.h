//===-- CDDebugInfo.h - CD source metadata helpers --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CD_CDDEBUGINFO_H
#define LLVM_LIB_TARGET_CD_CDDEBUGINFO_H

#include "CDBytecodeFormat.h"

#include <string>
#include <vector>

namespace llvm {
class Module;

namespace cd {

/// Parse the optional !cd.sources named metadata into the typed artifact
/// source table. An absent node is valid and produces an empty table.
bool parseCDSources(const Module &M, std::vector<CDDebugSource> &Sources,
                    std::string &Error);

} // namespace cd
} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDDEBUGINFO_H
