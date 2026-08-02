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

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/DebugLoc.h"

#include <optional>
#include <string>
#include <vector>

namespace llvm {
class DILocation;
class Module;

namespace cd {

/// Parse the optional !cd.sources named metadata into the typed artifact
/// source table. An absent node is valid and produces an empty table.
bool parseCDSources(const Module &M, std::vector<CDDebugSource> &Sources,
                    std::string &Error);

struct CDDebugRangeMetadata {
  const DILocation *location = nullptr;
  CDDebugRange range;
};

/// Parse optional !cd.ranges records keyed by DILocation metadata. An absent
/// node is valid and produces an empty table.
bool parseCDDebugRanges(const Module &M, ArrayRef<CDDebugSource> Sources,
                        std::vector<CDDebugRangeMetadata> &Ranges,
                        std::string &Error);

/// Resolve an LLVM instruction debug location to an explicit CD source.
/// Locations without a source table match or without a representable positive
/// line/column are intentionally omitted from the additive artifact section.
bool resolveCDDebugLocation(const DebugLoc &Location,
                            ArrayRef<CDDebugSource> Sources,
                            std::optional<CDDebugLocation> &Resolved,
                            std::string &Error);

/// Resolve an LLVM debug location and attach its explicit source range, when
/// the module supplied a matching !cd.ranges record.
bool resolveCDDebugLocation(const DebugLoc &Location,
                            ArrayRef<CDDebugSource> Sources,
                            ArrayRef<CDDebugRangeMetadata> Ranges,
                            std::optional<CDDebugLocation> &Resolved,
                            std::string &Error);

} // namespace cd
} // namespace llvm

#endif // LLVM_LIB_TARGET_CD_CDDEBUGINFO_H
