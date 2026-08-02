//===-- CDDebugInfo.cpp - CD source metadata helpers ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDDebugInfo.h"

#include "llvm/ADT/Twine.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/JSON.h"

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

} // namespace llvm::cd
