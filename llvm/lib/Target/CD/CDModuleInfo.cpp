//===-- CDModuleInfo.cpp - CD module metadata helpers ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDModuleInfo.h"

#include "llvm/ADT/Twine.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/JSON.h"

using namespace llvm;

namespace llvm::cd {

static bool fail(std::string &Error, Twine Message) {
  Error = Message.str();
  return false;
}

bool hasCDModuleMetadata(const Module &M) {
  return M.getNamedMetadata("cd.module") ||
         M.getNamedMetadata("cd.dependencies");
}

static bool parseString(const Metadata *Operand, StringRef Prefix,
                        unsigned OperandIndex, std::string &Value,
                        std::string &Error) {
  const auto *String = dyn_cast_or_null<MDString>(Operand);
  if (!String)
    return fail(Error, Prefix + " operand " + Twine(OperandIndex) +
                         " must be a string");
  if (!json::isUTF8(String->getString()))
    return fail(Error, Prefix + " operand " + Twine(OperandIndex) +
                         " contains invalid UTF-8");
  Value = String->getString().str();
  return true;
}

static bool parseUnsigned(const Metadata *Operand, StringRef Prefix,
                          unsigned OperandIndex, uint64_t &Value,
                          std::string &Error) {
  const auto *Constant = dyn_cast_or_null<ConstantAsMetadata>(Operand);
  const auto *Integer =
      Constant ? dyn_cast<ConstantInt>(Constant->getValue()) : nullptr;
  if (!Integer || Integer->getValue().isNegative() ||
      Integer->getValue().getActiveBits() > 64)
    return fail(Error, Prefix + " operand " + Twine(OperandIndex) +
                         " must be a non-negative 64-bit integer");
  Value = Integer->getValue().getZExtValue();
  return true;
}

static bool parseEntryFlag(const Metadata *Operand, StringRef Prefix,
                           unsigned OperandIndex, bool &Value,
                           std::string &Error) {
  const auto *Constant = dyn_cast_or_null<ConstantAsMetadata>(Operand);
  const auto *Integer =
      Constant ? dyn_cast<ConstantInt>(Constant->getValue()) : nullptr;
  if (!Integer || !Integer->getType()->isIntegerTy(1))
    return fail(Error, Prefix + " operand " + Twine(OperandIndex) +
                         " must be an i1 constant");
  Value = Integer->isOne();
  return true;
}

bool parseCDModuleMetadata(const Module &M, CDModuleMetadata &Metadata,
                           std::string &Error) {
  Metadata = {};
  const NamedMDNode *ModuleNode = M.getNamedMetadata("cd.module");
  if (!ModuleNode)
    return fail(Error, "module artifact requires !cd.module metadata");
  if (ModuleNode->getNumOperands() != 1)
    return fail(Error, "module metadata llvm.cd.module must contain exactly one record");

  const MDNode *Record = ModuleNode->getOperand(0);
  if (!Record)
    return fail(Error, "module metadata llvm.cd.module record 0 is null");
  const StringRef Prefix = "module metadata llvm.cd.module record 0";
  if (Record->getNumOperands() != 4 && Record->getNumOperands() != 5)
    return fail(Error, Prefix + " must contain four or five operands");
  if (!parseString(Record->getOperand(0), Prefix, 0, Metadata.identity, Error) ||
      !parseString(Record->getOperand(1), Prefix, 1, Metadata.path, Error) ||
      !parseString(Record->getOperand(2), Prefix, 2, Metadata.canonicalPath,
                   Error) ||
      !parseEntryFlag(Record->getOperand(3), Prefix, 3, Metadata.isEntry,
                      Error))
    return false;
  if (Metadata.identity.empty() || Metadata.path.empty() ||
      Metadata.canonicalPath.empty())
    return fail(Error, Prefix + " identity and paths must not be empty");

  if (Record->getNumOperands() == 5) {
    uint64_t EntryOrder = 0;
    if (!parseUnsigned(Record->getOperand(4), Prefix, 4, EntryOrder, Error))
      return false;
    if (!Metadata.isEntry)
      return fail(Error, Prefix + " entry_order requires entry=true");
    Metadata.entryOrder = EntryOrder;
  } else if (Metadata.isEntry) {
    return fail(Error, Prefix + " entry=true requires entry_order");
  }

  const NamedMDNode *Dependencies = M.getNamedMetadata("cd.dependencies");
  if (!Dependencies)
    return true;

  for (unsigned RecordIndex = 0;
       RecordIndex < Dependencies->getNumOperands(); ++RecordIndex) {
    const MDNode *DependencyRecord = Dependencies->getOperand(RecordIndex);
    const std::string DependencyPrefix =
        (Twine("module metadata llvm.cd.dependencies record ") +
         Twine(RecordIndex))
            .str();
    if (!DependencyRecord)
      return fail(Error, DependencyPrefix + " is null");
    if (DependencyRecord->getNumOperands() != 4)
      return fail(Error, DependencyPrefix + " must contain four operands");

    std::string Kind;
    CDModuleDependency Dependency;
    if (!parseString(DependencyRecord->getOperand(0), DependencyPrefix, 0,
                     Kind, Error) ||
        !parseString(DependencyRecord->getOperand(1), DependencyPrefix, 1,
                     Dependency.identity, Error) ||
        !parseUnsigned(DependencyRecord->getOperand(2), DependencyPrefix, 2,
                       Dependency.instructionOffset, Error) ||
        !parseString(DependencyRecord->getOperand(3), DependencyPrefix, 3,
                     Dependency.requestedPath, Error))
      return false;
    if (Kind == "import")
      Dependency.kind = CDModuleDependencyKind::Import;
    else if (Kind == "re_export")
      Dependency.kind = CDModuleDependencyKind::ReExport;
    else
      return fail(Error, DependencyPrefix + " has unsupported dependency kind");
    if (Dependency.identity.empty() || Dependency.requestedPath.empty())
      return fail(Error, DependencyPrefix +
                           " identity and requested path must not be empty");
    Metadata.dependencies.push_back(std::move(Dependency));
  }
  return true;
}

} // namespace llvm::cd
