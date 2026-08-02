//===-- CDDebugInfo.cpp - CD source metadata helpers ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDDebugInfo.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Path.h"

#include <set>

using namespace llvm;

namespace llvm::cd {

static bool fail(std::string &Error, Twine Message) {
  Error = Message.str();
  return false;
}

bool parseCDSources(const Module &M, std::vector<CDDebugSource> &Sources,
                    std::string &Error) {
  Sources.clear();
  const NamedMDNode *Named = M.getNamedMetadata("cd.sources");
  if (!Named)
    return true;

  std::set<std::string> Identities;
  for (unsigned RecordIndex = 0; RecordIndex < Named->getNumOperands();
       ++RecordIndex) {
    const MDNode *Record = Named->getOperand(RecordIndex);
    if (!Record)
      return fail(Error, Twine("llvm.cd.sources record ") +
                           Twine(RecordIndex) + " is null");

    const unsigned OperandCount = Record->getNumOperands();
    if (OperandCount != 2 && OperandCount != 3)
      return fail(Error, Twine("llvm.cd.sources record ") +
                           Twine(RecordIndex) +
                           " must contain two or three string operands");

    SmallVector<StringRef, 3> Values;
    Values.reserve(OperandCount);
    for (unsigned OperandIndex = 0; OperandIndex < OperandCount;
         ++OperandIndex) {
      const Metadata *Operand = Record->getOperand(OperandIndex);
      const auto *String = dyn_cast_or_null<MDString>(Operand);
      if (!String)
        return fail(Error, Twine("llvm.cd.sources record ") +
                             Twine(RecordIndex) + " operand " +
                             Twine(OperandIndex) + " must be a string");
      const StringRef Value = String->getString();
      if (!json::isUTF8(Value))
        return fail(Error, Twine("llvm.cd.sources record ") +
                             Twine(RecordIndex) +
                             " contains invalid UTF-8");
      Values.push_back(Value);
    }

    const bool HasModule = OperandCount == 3;
    const StringRef Module = HasModule ? Values[0] : StringRef();
    const StringRef Path = HasModule ? Values[1] : Values[0];
    const StringRef Text = HasModule ? Values[2] : Values[1];
    if (HasModule && Module.empty())
      return fail(Error, Twine("llvm.cd.sources record ") +
                           Twine(RecordIndex) +
                           " module identity must not be empty");
    if (Path.empty())
      return fail(Error, Twine("llvm.cd.sources record ") +
                           Twine(RecordIndex) + " path must not be empty");

    std::string Identity = Module.str();
    Identity.push_back('\0');
    Identity += Path.str();
    if (!Identities.insert(Identity).second)
      return fail(Error, Twine("llvm.cd.sources record ") +
                           Twine(RecordIndex) +
                           " duplicates source identity");

    CDDebugSource Source;
    if (HasModule)
      Source.module = Module.str();
    Source.path = Path.str();
    Source.text = Text.str();
    Sources.push_back(std::move(Source));
  }
  return true;
}

static bool findCDSource(ArrayRef<CDDebugSource> Sources, const DIFile &File,
                         unsigned &SourceIndex, std::string &Error) {
  SmallString<256> QualifiedPath(File.getDirectory());
  if (!File.getDirectory().empty())
    sys::path::append(QualifiedPath, File.getFilename());

  const StringRef Candidates[] = {File.getFilename(), QualifiedPath};
  for (StringRef Candidate : Candidates) {
    if (Candidate.empty())
      continue;

    unsigned Match = InvalidIndex;
    unsigned MatchCount = 0;
    for (unsigned Index = 0; Index < Sources.size(); ++Index) {
      if (Sources[Index].path != Candidate)
        continue;
      Match = Index;
      ++MatchCount;
    }
    if (MatchCount > 1)
      return fail(Error, Twine("debug location file `") + Candidate +
                           " matches multiple CD sources");
    if (MatchCount == 1) {
      SourceIndex = Match;
      return true;
    }
  }

  SourceIndex = InvalidIndex;
  return true;
}

bool resolveCDDebugLocation(const DebugLoc &Location,
                            ArrayRef<CDDebugSource> Sources,
                            std::optional<CDDebugLocation> &Resolved,
                            std::string &Error) {
  return resolveCDDebugLocation(Location, Sources, {}, Resolved, Error);
}

static bool parseCDRangeOffset(const Metadata *Operand, unsigned RecordIndex,
                               unsigned OperandIndex, uint64_t &Offset,
                               std::string &Error) {
  const auto *Constant = dyn_cast_or_null<ConstantAsMetadata>(Operand);
  const auto *Integer =
      Constant ? dyn_cast<ConstantInt>(Constant->getValue()) : nullptr;
  if (!Integer || Integer->getValue().isNegative() ||
      Integer->getValue().getActiveBits() > 64)
    return fail(Error, Twine("llvm.cd.ranges record ") + Twine(RecordIndex) +
                         " operand " + Twine(OperandIndex) +
                         " must be a non-negative 64-bit integer byte offset");
  Offset = Integer->getZExtValue();
  return true;
}

bool parseCDDebugRanges(const Module &M, ArrayRef<CDDebugSource> Sources,
                        std::vector<CDDebugRangeMetadata> &Ranges,
                        std::string &Error) {
  Ranges.clear();
  const NamedMDNode *Named = M.getNamedMetadata("cd.ranges");
  if (!Named)
    return true;

  std::set<const DILocation *> Locations;
  for (unsigned RecordIndex = 0; RecordIndex < Named->getNumOperands();
       ++RecordIndex) {
    const MDNode *Record = Named->getOperand(RecordIndex);
    if (!Record)
      return fail(Error, Twine("llvm.cd.ranges record ") +
                           Twine(RecordIndex) + " is null");
    if (Record->getNumOperands() != 3)
      return fail(Error, Twine("llvm.cd.ranges record ") +
                           Twine(RecordIndex) +
                           " must contain a DILocation and two byte offsets");

    const auto *Location =
        dyn_cast_or_null<DILocation>(Record->getOperand(0));
    if (!Location)
      return fail(Error, Twine("llvm.cd.ranges record ") +
                           Twine(RecordIndex) +
                           " operand 0 must be a DILocation");

    uint64_t Start = 0;
    uint64_t End = 0;
    if (!parseCDRangeOffset(Record->getOperand(1), RecordIndex, 1, Start,
                            Error) ||
        !parseCDRangeOffset(Record->getOperand(2), RecordIndex, 2, End,
                            Error))
      return false;
    if (Start > End)
      return fail(Error, Twine("llvm.cd.ranges record ") +
                           Twine(RecordIndex) + " has a reversed byte range");
    if (!Locations.insert(Location).second)
      return fail(Error, Twine("llvm.cd.ranges record ") +
                           Twine(RecordIndex) +
                           " duplicates a DILocation range");

    std::optional<CDDebugLocation> Resolved;
    if (!resolveCDDebugLocation(DebugLoc(const_cast<DILocation *>(Location)),
                                Sources, Resolved, Error))
      return false;
    if (!Resolved)
      return fail(Error, Twine("llvm.cd.ranges record ") +
                           Twine(RecordIndex) +
                           " does not resolve to an explicit source-backed location");
    if (End > Sources[Resolved->source].text.size())
      return fail(Error, Twine("llvm.cd.ranges record ") +
                           Twine(RecordIndex) +
                           " exceeds the source text length");

    CDDebugRangeMetadata Metadata;
    Metadata.location = Location;
    Metadata.range = {Resolved->source, Start, End};
    Ranges.push_back(Metadata);
  }
  return true;
}

bool resolveCDDebugLocation(const DebugLoc &Location,
                            ArrayRef<CDDebugSource> Sources,
                            ArrayRef<CDDebugRangeMetadata> Ranges,
                            std::optional<CDDebugLocation> &Resolved,
                            std::string &Error) {
  Resolved.reset();
  if (!Location || Sources.empty())
    return true;

  const DILocation *DILocationValue = Location.get();
  if (!DILocationValue || DILocationValue->getLine() == 0 ||
      DILocationValue->getColumn() == 0)
    return true;

  const DILocalScope *Scope = DILocationValue->getScope();
  const DIFile *File = Scope ? Scope->getFile() : nullptr;
  if (!File)
    return true;

  unsigned SourceIndex = InvalidIndex;
  if (!findCDSource(Sources, *File, SourceIndex, Error))
    return false;
  if (SourceIndex == InvalidIndex)
    return true;

  Resolved = CDDebugLocation{SourceIndex, DILocationValue->getLine(),
                             DILocationValue->getColumn(), std::nullopt};
  for (const CDDebugRangeMetadata &Metadata : Ranges) {
    if (Metadata.location != DILocationValue)
      continue;
    if (Metadata.range.source != SourceIndex)
      return fail(Error, "debug range source does not match debug location");
    Resolved->range = Metadata.range;
    break;
  }
  return true;
}

} // namespace llvm::cd
