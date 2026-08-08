//===-- CDBytecodeFormat.cpp - CD bytecode artifact model ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDBytecodeFormat.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include <cmath>
#include <set>
#include <utility>

using namespace llvm;

namespace {

using namespace llvm::cd;

static std::string numberText(double Number) {
  std::string Text;
  raw_string_ostream Stream(Text);
  Stream << format("%.17g", Number);
  Stream.flush();
  return Text;
}

static std::string escapeString(StringRef Value) {
  std::string Result;
  Result.reserve(Value.size() + 2);
  for (char Character : Value) {
    switch (Character) {
    case '\\':
      Result += "\\\\";
      break;
    case '"':
      Result += "\\\"";
      break;
    case '\n':
      Result += "\\n";
      break;
    case '\r':
      Result += "\\r";
      break;
    case '\t':
      Result += "\\t";
      break;
    default:
      Result += Character;
      break;
    }
  }
  return Result;
}

static void writeQuoted(raw_ostream &OS, StringRef Value) {
  OS << '"' << escapeString(Value) << '"';
}

static std::string registerName(unsigned Register) {
  return "r" + std::to_string(Register);
}

static std::string constantName(unsigned Constant) {
  return "c" + std::to_string(Constant);
}

static std::string nameName(unsigned Name) {
  return "n" + std::to_string(Name);
}

static std::string functionName(unsigned Function) {
  return "f" + std::to_string(Function);
}

static bool fail(std::string &Error, Twine Message) {
  Error = Message.str();
  return false;
}

static bool isSupportedNativeName(StringRef Name) {
  return Name == "floor" || Name == "ceil" || Name == "sqrt" ||
         Name == "str" || Name == "typeOf" || Name == "hash" ||
         Name == "range" || Name == "substr" || Name == "charAt" ||
         Name == "map" || Name == "filter" || Name == "any" ||
         Name == "all" || Name == "count";
}

static bool validateRegister(const CDBody &Body, unsigned Register,
                             StringRef BodyName, unsigned InstructionIndex,
                             StringRef Role, std::string &Error) {
  if (Register == InvalidIndex || Register >= Body.registers)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has " + Role +
                         " register outside the body register range");
  return true;
}

static bool validateReference(unsigned Reference, unsigned Count,
                              StringRef Kind, StringRef BodyName,
                              unsigned InstructionIndex, std::string &Error) {
  if (Reference == InvalidIndex || Reference >= Count)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an out-of-range " +
                         Kind + " reference");
  return true;
}

static bool validateResult(const CDInstruction &Instruction,
                           const CDBody &Body, StringRef BodyName,
                           unsigned InstructionIndex, std::string &Error) {
  if (Instruction.result == InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " is missing a result register");
  return validateRegister(Body, Instruction.result, BodyName, InstructionIndex,
                          "result", Error);
}

static bool validateNoResult(const CDInstruction &Instruction,
                             StringRef BodyName, unsigned InstructionIndex,
                             std::string &Error) {
  if (Instruction.result != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an unexpected result register");
  return true;
}

static bool validateOperandCount(const CDInstruction &Instruction,
                                 size_t Count, StringRef BodyName,
                                 unsigned InstructionIndex,
                                 std::string &Error) {
  if (Instruction.operands.size() != Count)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has the wrong operand count");
  return true;
}

static bool validateUnusedFieldsWithVariantFields(
    const CDInstruction &Instruction, bool HasReference, bool HasCallee,
    bool HasTarget, bool HasSecondaryReference, bool HasIndex,
    StringRef BodyName, unsigned InstructionIndex, std::string &Error) {
  if (!HasReference && Instruction.reference != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) +
                         " has an unexpected table reference");
  if (!HasCallee && Instruction.callee != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) +
                         " has an unexpected callee register");
  if (!HasTarget && Instruction.target != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) +
                         " has an unexpected jump target");
  if (!HasSecondaryReference && Instruction.secondaryReference != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) +
                         " has an unexpected secondary table reference");
  if (!HasIndex && Instruction.payloadIndex != InvalidIndex)
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an unexpected index");
  return true;
}

static bool validateUnusedFields(const CDInstruction &Instruction,
                                 bool HasReference, bool HasCallee,
                                 bool HasTarget, StringRef BodyName,
                                 unsigned InstructionIndex,
                                 std::string &Error) {
  return validateUnusedFieldsWithVariantFields(
      Instruction, HasReference, HasCallee, HasTarget,
      /*HasSecondaryReference=*/false, /*HasIndex=*/false, BodyName,
      InstructionIndex, Error);
}

static bool validateOperands(const CDInstruction &Instruction,
                             const CDBody &Body, StringRef BodyName,
                             unsigned InstructionIndex, std::string &Error) {
  for (unsigned Register : Instruction.operands)
    if (!validateRegister(Body, Register, BodyName, InstructionIndex, "operand",
                          Error))
      return false;
  return true;
}

static bool validateInstruction(const CDInstruction &Instruction,
                                const CDArtifact &Artifact,
                                const CDBody &Body, StringRef BodyName,
                                unsigned InstructionIndex, std::string &Error) {
  switch (Instruction.opcode) {
  case CDOpcode::Constant:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.constants.size(),
                           "constant", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 0, BodyName, InstructionIndex,
                              Error))
      return false;
    return true;
  case CDOpcode::MakeFunction:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.functions.size(),
                           "function", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 0, BodyName, InstructionIndex,
                              Error))
      return false;
    return true;
  case CDOpcode::Array:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Map:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    if (Instruction.operands.size() % 2 != 0)
      return fail(Error, Twine(BodyName) + " instruction " +
                           Twine(InstructionIndex) +
                           " map requires key/value operand pairs");
    if (!validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Struct:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    if (Instruction.operands.size() % 2 != 0)
      return fail(Error, Twine(BodyName) + " instruction " +
                           Twine(InstructionIndex) +
                           " struct requires field name/value operand pairs");
    if (Instruction.reference != InvalidIndex &&
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "struct type name", BodyName, InstructionIndex,
                           Error))
      return false;
    for (unsigned Index = 0; Index < Instruction.operands.size(); Index += 2) {
      if (!validateReference(Instruction.operands[Index], Artifact.names.size(),
                             "struct field name", BodyName, InstructionIndex,
                             Error) ||
          !validateRegister(Body, Instruction.operands[Index + 1], BodyName,
                            InstructionIndex, "struct field value", Error))
        return false;
    }
    return true;
  case CDOpcode::Variant:
    if (!validateUnusedFieldsWithVariantFields(
            Instruction, true, false, false,
            /*HasSecondaryReference=*/true, /*HasIndex=*/false, BodyName,
            InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "variant enum name", BodyName, InstructionIndex,
                           Error) ||
        !validateReference(Instruction.secondaryReference,
                           Artifact.names.size(), "variant name", BodyName,
                           InstructionIndex, Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex,
                          Error))
      return false;
    return true;
  case CDOpcode::VariantTag:
    if (!validateUnusedFieldsWithVariantFields(
            Instruction, true, false, false,
            /*HasSecondaryReference=*/true, /*HasIndex=*/false, BodyName,
            InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "variant enum name", BodyName, InstructionIndex,
                           Error) ||
        !validateReference(Instruction.secondaryReference,
                           Artifact.names.size(), "variant name", BodyName,
                           InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex,
                          Error))
      return false;
    return true;
  case CDOpcode::VariantField:
    if (!validateUnusedFieldsWithVariantFields(
            Instruction, false, false, false,
            /*HasSecondaryReference=*/false, /*HasIndex=*/true, BodyName,
            InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex,
                          Error))
      return false;
    return true;
  case CDOpcode::Field:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "field name", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::AssignField:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "field name", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 2, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Index:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 2, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::AssignIndex:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 3, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Len:
  case CDOpcode::AssertArray:
  case CDOpcode::Move:
  case CDOpcode::Negate:
  case CDOpcode::Not:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::LoadVar:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "name", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 0, BodyName, InstructionIndex,
                              Error))
      return false;
    return true;
  case CDOpcode::StoreVar:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateNoResult(Instruction, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "name", BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Call:
    if (!validateUnusedFields(Instruction, false, true, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateRegister(Body, Instruction.callee, BodyName, InstructionIndex,
                          "callee", Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::NativeCall:
    if (!validateUnusedFields(Instruction, true, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateReference(Instruction.reference, Artifact.names.size(),
                           "native name", BodyName, InstructionIndex, Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex,
                          Error))
      return false;
    if (!isSupportedNativeName(Artifact.names[Instruction.reference]))
      return fail(Error, Twine(BodyName) + " instruction " +
                           Twine(InstructionIndex) +
                           " has an unsupported native name");
    return true;
  case CDOpcode::Print:
  case CDOpcode::Return:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateNoResult(Instruction, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Add:
  case CDOpcode::Subtract:
  case CDOpcode::Multiply:
  case CDOpcode::Divide:
  case CDOpcode::Equal:
  case CDOpcode::NotEqual:
  case CDOpcode::Greater:
  case CDOpcode::GreaterEqual:
  case CDOpcode::Less:
  case CDOpcode::LessEqual:
    if (!validateUnusedFields(Instruction, false, false, false, BodyName,
                              InstructionIndex, Error) ||
        !validateResult(Instruction, Body, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 2, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    return true;
  case CDOpcode::Jump:
    if (!validateUnusedFields(Instruction, false, false, true, BodyName,
                              InstructionIndex, Error) ||
        !validateNoResult(Instruction, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 0, BodyName, InstructionIndex,
                              Error))
      return false;
    break;
  case CDOpcode::JumpIfFalse:
  case CDOpcode::JumpIfTrue:
    if (!validateUnusedFields(Instruction, false, false, true, BodyName,
                              InstructionIndex, Error) ||
        !validateNoResult(Instruction, BodyName, InstructionIndex, Error) ||
        !validateOperandCount(Instruction, 1, BodyName, InstructionIndex,
                              Error) ||
        !validateOperands(Instruction, Body, BodyName, InstructionIndex, Error))
      return false;
    break;
  default:
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an unknown opcode");
  }

  if (Instruction.target > Body.instructions.size())
    return fail(Error, Twine(BodyName) + " instruction " +
                         Twine(InstructionIndex) + " has an out-of-range jump target");
  return true;
}

static bool validateBody(const CDArtifact &Artifact, const CDBody &Body,
                         StringRef BodyName, std::string &Error) {
  if (!Body.locations.empty() &&
      Body.locations.size() != Body.instructions.size())
    return fail(Error, Twine(BodyName) + " has " +
                         Twine(Body.locations.size()) +
                         " debug locations for " +
                         Twine(Body.instructions.size()) + " instructions");

  for (unsigned Index = 0; Index < Body.locations.size(); ++Index) {
    const std::optional<CDDebugLocation> &Location = Body.locations[Index];
    if (!Location)
      continue;
    if (Location->source >= Artifact.debugSources.size())
      return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                           " has a debug location source outside the source table");
    if (Location->line == 0 || Location->column == 0)
      return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                           " has a non-positive debug location");
    if (Location->range) {
      const CDDebugRange &Range = *Location->range;
      if (Range.source >= Artifact.debugSources.size())
        return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                             " has a debug range source outside the source table");
      if (Range.source != Location->source)
        return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                             " debug range source does not match its location");
      if (Range.start > Range.end)
        return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                             " has a reversed debug range");
      if (Range.end > Artifact.debugSources[Range.source].text.size())
        return fail(Error, Twine(BodyName) + " instruction " + Twine(Index) +
                             " debug range exceeds source length");
    }
  }

  for (unsigned Index = 0; Index < Body.instructions.size(); ++Index)
    if (!validateInstruction(Body.instructions[Index], Artifact, Body, BodyName,
                             Index, Error))
      return false;
  return true;
}

static bool validateConstants(const CDArtifact &Artifact, std::string &Error) {
  for (unsigned Index = 0; Index < Artifact.constants.size(); ++Index) {
    const CDConstant &Constant = Artifact.constants[Index];
    switch (Constant.kind) {
    case CDConstant::Nil:
      if (!Constant.text.empty())
        return fail(Error, Twine("constant c") + Twine(Index) +
                             " has unexpected nil payload");
      break;
    case CDConstant::Bool:
      if (Constant.text != "true" && Constant.text != "false")
        return fail(Error, Twine("constant c") + Twine(Index) +
                             " has an invalid boolean payload");
      break;
    case CDConstant::Number: {
      double Number = 0.0;
      if (StringRef(Constant.text).getAsDouble(Number) ||
          !std::isfinite(Number))
        return fail(Error, Twine("constant c") + Twine(Index) +
                             " is not a finite number");
      break;
    }
    case CDConstant::String:
      if (!json::isUTF8(Constant.text))
        return fail(Error, Twine("constant c") + Twine(Index) +
                             " is not valid UTF-8");
      break;
    default:
      return fail(Error, Twine("constant c") + Twine(Index) +
                           " has an unknown kind");
    }
  }
  return true;
}

static bool validateDebugSources(const CDArtifact &Artifact,
                                 std::string &Error) {
  std::set<std::string> Identities;
  for (unsigned Index = 0; Index < Artifact.debugSources.size(); ++Index) {
    const CDDebugSource &Source = Artifact.debugSources[Index];
    if (Source.module && Source.module->empty())
      return fail(Error, Twine("debug source s") + Twine(Index) +
                           " has an empty module identity");
    if (Source.path.empty())
      return fail(Error, Twine("debug source s") + Twine(Index) +
                           " has an empty path");
    if (!json::isUTF8(Source.path) || !json::isUTF8(Source.text) ||
        (Source.module && !json::isUTF8(*Source.module)))
      return fail(Error, Twine("debug source s") + Twine(Index) +
                           " is not valid UTF-8");

    std::string Identity = Source.module.value_or("");
    Identity.push_back('\0');
    Identity += Source.path;
    if (!Identities.insert(Identity).second)
      return fail(Error, Twine("debug source s") + Twine(Index) +
                           " duplicates source identity");
  }
  return true;
}

static bool validateModuleMetadata(const CDArtifact &Artifact,
                                   std::string &Error) {
  if (!Artifact.module)
    return true;

  const CDModuleMetadata &Module = *Artifact.module;
  if (Module.identity.empty())
    return fail(Error, "module identity must not be empty");
  if (Module.path.empty())
    return fail(Error, "module path must not be empty");
  if (Module.canonicalPath.empty())
    return fail(Error, "module canonical path must not be empty");
  if (!json::isUTF8(Module.identity) || !json::isUTF8(Module.path) ||
      !json::isUTF8(Module.canonicalPath))
    return fail(Error, "module metadata contains invalid UTF-8");
  if (Module.isEntry != Module.entryOrder.has_value())
    return fail(Error,
                "module entry_order must be present exactly for entry modules");

  uint64_t PreviousOffset = 0;
  for (unsigned Index = 0; Index < Module.dependencies.size(); ++Index) {
    const CDModuleDependency &Dependency = Module.dependencies[Index];
    if (Dependency.identity.empty() || Dependency.requestedPath.empty())
      return fail(Error, Twine("module dependency d") + Twine(Index) +
                           " has an empty identity or path");
    if (!json::isUTF8(Dependency.identity) ||
        !json::isUTF8(Dependency.requestedPath))
      return fail(Error, Twine("module dependency d") + Twine(Index) +
                           " contains invalid UTF-8");
    if (Dependency.instructionOffset > Artifact.main.instructions.size())
      return fail(Error, Twine("module dependency d") + Twine(Index) +
                           " instruction offset out of range");
    if (Index != 0 && Dependency.instructionOffset < PreviousOffset)
      return fail(Error, "module dependency offsets must be nondecreasing");
    PreviousOffset = Dependency.instructionOffset;
  }
  return true;
}

static void writeOperands(raw_ostream &OS, ArrayRef<unsigned> Operands) {
  for (unsigned Index = 0; Index < Operands.size(); ++Index) {
    if (Index != 0)
      OS << ", ";
    OS << registerName(Operands[Index]);
  }
}

static void writeInstruction(raw_ostream &OS, const CDInstruction &Instruction) {
  OS << "  ";
  if (Instruction.result != InvalidIndex)
    OS << registerName(Instruction.result) << " = ";

  switch (Instruction.opcode) {
  case CDOpcode::Constant:
    OS << "constant " << constantName(Instruction.reference);
    break;
  case CDOpcode::MakeFunction:
    OS << "make_function " << functionName(Instruction.reference);
    break;
  case CDOpcode::Array:
    OS << "array [";
    writeOperands(OS, Instruction.operands);
    OS << "]";
    break;
  case CDOpcode::Map:
    OS << "map [";
    for (unsigned Index = 0; Index < Instruction.operands.size(); Index += 2) {
      if (Index != 0)
        OS << ", ";
      OS << registerName(Instruction.operands[Index]) << ": "
         << registerName(Instruction.operands[Index + 1]);
    }
    OS << "]";
    break;
  case CDOpcode::Struct:
    OS << "struct ";
    if (Instruction.reference != InvalidIndex)
      OS << nameName(Instruction.reference) << " ";
    OS << "{";
    for (unsigned Index = 0; Index < Instruction.operands.size(); Index += 2) {
      if (Index != 0)
        OS << ", ";
      OS << nameName(Instruction.operands[Index]) << ": "
         << registerName(Instruction.operands[Index + 1]);
    }
    OS << "}";
    break;
  case CDOpcode::Variant:
    OS << "variant " << nameName(Instruction.reference) << "."
       << nameName(Instruction.secondaryReference) << " [";
    writeOperands(OS, Instruction.operands);
    OS << "]";
    break;
  case CDOpcode::VariantTag:
    OS << "variant_tag " << registerName(Instruction.operands[0]) << " "
       << nameName(Instruction.reference) << "."
       << nameName(Instruction.secondaryReference);
    break;
  case CDOpcode::VariantField:
    OS << "variant_field " << registerName(Instruction.operands[0]) << " "
       << Instruction.payloadIndex;
    break;
  case CDOpcode::Field:
    OS << "field " << registerName(Instruction.operands[0]) << ", "
       << nameName(Instruction.reference);
    break;
  case CDOpcode::AssignField:
    OS << "assign_field " << registerName(Instruction.operands[0]) << ", "
       << nameName(Instruction.reference) << ", "
       << registerName(Instruction.operands[1]);
    break;
  case CDOpcode::Index:
    OS << "index ";
    writeOperands(OS, Instruction.operands);
    break;
  case CDOpcode::AssignIndex:
    OS << "assign_index ";
    writeOperands(OS, Instruction.operands);
    break;
  case CDOpcode::Len:
    OS << "len " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::AssertArray:
    OS << "assert_array " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Move:
    OS << "move " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::LoadVar:
    OS << "load_var " << nameName(Instruction.reference);
    break;
  case CDOpcode::StoreVar:
    OS << "store_var " << nameName(Instruction.reference) << ", "
       << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Call:
    OS << "call " << registerName(Instruction.callee) << " [";
    writeOperands(OS, Instruction.operands);
    OS << "]";
    break;
  case CDOpcode::NativeCall:
    OS << "native_call " << nameName(Instruction.reference) << " [";
    writeOperands(OS, Instruction.operands);
    OS << "]";
    break;
  case CDOpcode::Print:
    OS << "print " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Return:
    OS << "return " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Negate:
    OS << "negate " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Not:
    OS << "not " << registerName(Instruction.operands[0]);
    break;
  case CDOpcode::Add:
  case CDOpcode::Subtract:
  case CDOpcode::Multiply:
  case CDOpcode::Divide:
  case CDOpcode::Equal:
  case CDOpcode::NotEqual:
  case CDOpcode::Greater:
  case CDOpcode::GreaterEqual:
  case CDOpcode::Less:
  case CDOpcode::LessEqual:
    OS << opcodeName(Instruction.opcode) << " ";
    writeOperands(OS, Instruction.operands);
    break;
  case CDOpcode::Jump:
    OS << "jump " << Instruction.target;
    break;
  case CDOpcode::JumpIfFalse:
  case CDOpcode::JumpIfTrue:
    OS << opcodeName(Instruction.opcode) << " "
       << registerName(Instruction.operands[0]) << ", " << Instruction.target;
    break;
  }
  OS << '\n';
}

} // namespace

namespace llvm::cd {

CDConstant CDConstant::nil() { return {CDConstant::Nil, {}}; }

CDConstant CDConstant::number(double Value) {
  return {CDConstant::Number, numberText(Value)};
}

CDConstant CDConstant::boolean(bool Value) {
  return {CDConstant::Bool, Value ? "true" : "false"};
}

CDConstant CDConstant::string(StringRef Value) {
  return {CDConstant::String, Value.str()};
}

CDInstruction CDInstruction::constant(unsigned Destination, unsigned Constant) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Constant;
  Instruction.result = Destination;
  Instruction.reference = Constant;
  return Instruction;
}

CDInstruction CDInstruction::makeFunction(unsigned Destination,
                                           unsigned Function) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::MakeFunction;
  Instruction.result = Destination;
  Instruction.reference = Function;
  return Instruction;
}

CDInstruction CDInstruction::array(unsigned Destination,
                                    std::vector<unsigned> Elements) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Array;
  Instruction.result = Destination;
  Instruction.operands = std::move(Elements);
  return Instruction;
}

CDInstruction CDInstruction::map(unsigned Destination,
                                  std::vector<unsigned> KeyValueOperands) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Map;
  Instruction.result = Destination;
  Instruction.operands = std::move(KeyValueOperands);
  return Instruction;
}

CDInstruction CDInstruction::structValue(
    unsigned Destination, unsigned TypeName,
    std::vector<unsigned> FieldNameValueOperands) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Struct;
  Instruction.result = Destination;
  Instruction.reference = TypeName;
  Instruction.operands = std::move(FieldNameValueOperands);
  return Instruction;
}

CDInstruction CDInstruction::variant(unsigned Destination, unsigned EnumName,
                                     unsigned VariantName,
                                     std::vector<unsigned> Payload) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Variant;
  Instruction.result = Destination;
  Instruction.reference = EnumName;
  Instruction.secondaryReference = VariantName;
  Instruction.operands = std::move(Payload);
  return Instruction;
}

CDInstruction CDInstruction::variantTag(unsigned Destination, unsigned Value,
                                         unsigned EnumName,
                                         unsigned VariantName) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::VariantTag;
  Instruction.result = Destination;
  Instruction.reference = EnumName;
  Instruction.secondaryReference = VariantName;
  Instruction.operands = {Value};
  return Instruction;
}

CDInstruction CDInstruction::variantField(unsigned Destination, unsigned Value,
                                           unsigned Index) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::VariantField;
  Instruction.result = Destination;
  Instruction.payloadIndex = Index;
  Instruction.operands = {Value};
  return Instruction;
}

CDInstruction CDInstruction::field(unsigned Destination, unsigned Object,
                                   unsigned Name) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Field;
  Instruction.result = Destination;
  Instruction.reference = Name;
  Instruction.operands = {Object};
  return Instruction;
}

CDInstruction CDInstruction::assignField(unsigned Destination, unsigned Object,
                                         unsigned Name, unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::AssignField;
  Instruction.result = Destination;
  Instruction.reference = Name;
  Instruction.operands = {Object, Value};
  return Instruction;
}

CDInstruction CDInstruction::index(unsigned Destination, unsigned Collection,
                                    unsigned Index) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Index;
  Instruction.result = Destination;
  Instruction.operands = {Collection, Index};
  return Instruction;
}

CDInstruction CDInstruction::assignIndex(unsigned Destination,
                                         unsigned Collection, unsigned Index,
                                         unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::AssignIndex;
  Instruction.result = Destination;
  Instruction.operands = {Collection, Index, Value};
  return Instruction;
}

CDInstruction CDInstruction::len(unsigned Destination, unsigned Value) {
  return unary(CDOpcode::Len, Destination, Value);
}

CDInstruction CDInstruction::assertArray(unsigned Destination, unsigned Value) {
  return unary(CDOpcode::AssertArray, Destination, Value);
}

CDInstruction CDInstruction::move(unsigned Destination, unsigned Source) {
  return unary(CDOpcode::Move, Destination, Source);
}

CDInstruction CDInstruction::unary(CDOpcode Opcode, unsigned Destination,
                                   unsigned Source) {
  CDInstruction Instruction;
  Instruction.opcode = Opcode;
  Instruction.result = Destination;
  Instruction.operands.push_back(Source);
  return Instruction;
}

CDInstruction CDInstruction::binary(CDOpcode Opcode, unsigned Destination,
                                    unsigned Left, unsigned Right) {
  CDInstruction Instruction;
  Instruction.opcode = Opcode;
  Instruction.result = Destination;
  Instruction.operands = {Left, Right};
  return Instruction;
}

CDInstruction CDInstruction::loadVar(unsigned Destination, unsigned Name) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::LoadVar;
  Instruction.result = Destination;
  Instruction.reference = Name;
  return Instruction;
}

CDInstruction CDInstruction::storeVar(unsigned Name, unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::StoreVar;
  Instruction.reference = Name;
  Instruction.operands.push_back(Value);
  return Instruction;
}

CDInstruction CDInstruction::call(unsigned Destination, unsigned Callee,
                                  std::vector<unsigned> Arguments) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Call;
  Instruction.result = Destination;
  Instruction.callee = Callee;
  Instruction.operands = std::move(Arguments);
  return Instruction;
}

CDInstruction CDInstruction::nativeCall(unsigned Destination, unsigned Name,
                                         std::vector<unsigned> Arguments) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::NativeCall;
  Instruction.result = Destination;
  Instruction.reference = Name;
  Instruction.operands = std::move(Arguments);
  return Instruction;
}

CDInstruction CDInstruction::print(unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Print;
  Instruction.operands.push_back(Value);
  return Instruction;
}

CDInstruction CDInstruction::returnValue(unsigned Value) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Return;
  Instruction.operands.push_back(Value);
  return Instruction;
}

CDInstruction CDInstruction::jump(unsigned Target) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::Jump;
  Instruction.target = Target;
  return Instruction;
}

CDInstruction CDInstruction::jumpIfFalse(unsigned Condition, unsigned Target) {
  CDInstruction Instruction;
  Instruction.opcode = CDOpcode::JumpIfFalse;
  Instruction.operands.push_back(Condition);
  Instruction.target = Target;
  return Instruction;
}

const char *opcodeName(CDOpcode Opcode) {
  switch (Opcode) {
  case CDOpcode::Constant:
    return "constant";
  case CDOpcode::MakeFunction:
    return "make_function";
  case CDOpcode::Array:
    return "array";
  case CDOpcode::Map:
    return "map";
  case CDOpcode::Struct:
    return "struct";
  case CDOpcode::Variant:
    return "variant";
  case CDOpcode::VariantTag:
    return "variant_tag";
  case CDOpcode::VariantField:
    return "variant_field";
  case CDOpcode::Field:
    return "field";
  case CDOpcode::AssignField:
    return "assign_field";
  case CDOpcode::Index:
    return "index";
  case CDOpcode::AssignIndex:
    return "assign_index";
  case CDOpcode::Len:
    return "len";
  case CDOpcode::AssertArray:
    return "assert_array";
  case CDOpcode::Move:
    return "move";
  case CDOpcode::LoadVar:
    return "load_var";
  case CDOpcode::StoreVar:
    return "store_var";
  case CDOpcode::Call:
    return "call";
  case CDOpcode::NativeCall:
    return "native_call";
  case CDOpcode::Print:
    return "print";
  case CDOpcode::Return:
    return "return";
  case CDOpcode::Negate:
    return "negate";
  case CDOpcode::Not:
    return "not";
  case CDOpcode::Add:
    return "add";
  case CDOpcode::Subtract:
    return "subtract";
  case CDOpcode::Multiply:
    return "multiply";
  case CDOpcode::Divide:
    return "divide";
  case CDOpcode::Equal:
    return "equal";
  case CDOpcode::NotEqual:
    return "not_equal";
  case CDOpcode::Greater:
    return "greater";
  case CDOpcode::GreaterEqual:
    return "greater_equal";
  case CDOpcode::Less:
    return "less";
  case CDOpcode::LessEqual:
    return "less_equal";
  case CDOpcode::Jump:
    return "jump";
  case CDOpcode::JumpIfFalse:
    return "jump_if_false";
  case CDOpcode::JumpIfTrue:
    return "jump_if_true";
  }
  return "unknown";
}

bool validateArtifact(const CDArtifact &Artifact, std::string &Error) {
  if (!validateConstants(Artifact, Error))
    return false;
  if (!validateDebugSources(Artifact, Error))
    return false;
  if (!validateModuleMetadata(Artifact, Error))
    return false;

  for (unsigned Index = 0; Index < Artifact.names.size(); ++Index)
    if (Artifact.names[Index].empty())
      return fail(Error, Twine("name n") + Twine(Index) + " is empty");

  if (!Artifact.main.parameterNames.empty())
    return fail(Error, "main body must not have parameter records");
  if (!validateBody(Artifact, Artifact.main, "main", Error))
    return false;

  for (unsigned Index = 0; Index < Artifact.functions.size(); ++Index) {
    const CDFunction &Function = Artifact.functions[Index];
    if (Function.arity != Function.body.parameterNames.size())
      return fail(Error, Twine("function f") + Twine(Index) +
                           " arity does not match parameter records");
    if (!validateBody(Artifact, Function.body, functionName(Index), Error))
      return false;
  }
  Error.clear();
  return true;
}

void serializeArtifact(const CDArtifact &Artifact, raw_ostream &OS) {
  std::string Error;
  if (!validateArtifact(Artifact, Error))
    report_fatal_error(Twine("CD bytecode artifact is invalid: ") + Error);

  OS << "cdbc 0.1\n\n";
  if (Artifact.module) {
    const CDModuleMetadata &Module = *Artifact.module;
    OS << "artifact: module\n\nmodule:\n";
    OS << "  identity = ";
    writeQuoted(OS, Module.identity);
    OS << "\n  path = ";
    writeQuoted(OS, Module.path);
    OS << "\n  canonical_path = ";
    writeQuoted(OS, Module.canonicalPath);
    OS << "\n  entry = " << (Module.isEntry ? "true" : "false") << '\n';
    if (Module.entryOrder)
      OS << "  entry_order = " << *Module.entryOrder << '\n';
    OS << "  dependencies:\n";
    for (unsigned Index = 0; Index < Module.dependencies.size(); ++Index) {
      const CDModuleDependency &Dependency = Module.dependencies[Index];
      OS << "    d" << Index << " target=";
      writeQuoted(OS, Dependency.identity);
      OS << " kind="
         << (Dependency.kind == CDModuleDependencyKind::Import ? "import"
                                                                 : "re_export")
         << " at=" << Dependency.instructionOffset << " requested=";
      writeQuoted(OS, Dependency.requestedPath);
      OS << '\n';
    }
    OS << '\n';
  }
  OS << "constants:\n";
  for (unsigned Index = 0; Index < Artifact.constants.size(); ++Index) {
    OS << "  " << constantName(Index) << " = ";
    switch (Artifact.constants[Index].kind) {
    case CDConstant::Nil:
      OS << "nil";
      break;
    case CDConstant::Number: {
      double Number;
      StringRef(Artifact.constants[Index].text).getAsDouble(Number);
      OS << "number " << numberText(Number);
      break;
    }
    case CDConstant::Bool:
      OS << "bool " << Artifact.constants[Index].text;
      break;
    case CDConstant::String:
      OS << "string ";
      writeQuoted(OS, Artifact.constants[Index].text);
      break;
    }
    OS << '\n';
  }

  OS << "\nnames:\n";
  for (unsigned Index = 0; Index < Artifact.names.size(); ++Index) {
    OS << "  " << nameName(Index) << " = ";
    writeQuoted(OS, Artifact.names[Index]);
    OS << '\n';
  }

  OS << "\nmain registers=" << Artifact.main.registers << ":\n";
  for (const CDInstruction &Instruction : Artifact.main.instructions)
    writeInstruction(OS, Instruction);

  for (unsigned Index = 0; Index < Artifact.functions.size(); ++Index) {
    const CDFunction &Function = Artifact.functions[Index];
    OS << "\nfunction " << functionName(Index) << " name=";
    writeQuoted(OS, Function.name);
    OS << " arity=" << Function.arity
       << " registers=" << Function.body.registers << ":\n";
    for (unsigned Parameter = 0;
         Parameter < Function.body.parameterNames.size(); ++Parameter) {
      OS << "  param " << Parameter << " = ";
      writeQuoted(OS, Function.body.parameterNames[Parameter]);
      OS << '\n';
    }
    for (const CDInstruction &Instruction : Function.body.instructions)
      writeInstruction(OS, Instruction);
  }

  if (!Artifact.debugSources.empty()) {
    OS << "\ndebug_sources:\n";
    for (unsigned Index = 0; Index < Artifact.debugSources.size(); ++Index) {
      const CDDebugSource &Source = Artifact.debugSources[Index];
      OS << "  s" << Index << " ";
      if (Source.module) {
        OS << "module=";
        writeQuoted(OS, *Source.module);
        OS << " ";
      }
      OS << "path=";
      writeQuoted(OS, Source.path);
      OS << " text=";
      writeQuoted(OS, Source.text);
      OS << '\n';
    }
  }

  bool HasDebugLocations = false;
  for (const std::optional<CDDebugLocation> &Location : Artifact.main.locations)
    HasDebugLocations |= Location.has_value();
  for (const CDFunction &Function : Artifact.functions)
    for (const std::optional<CDDebugLocation> &Location : Function.body.locations)
      HasDebugLocations |= Location.has_value();

  if (HasDebugLocations) {
    OS << "\ndebug_locations:\n";
    for (unsigned Index = 0; Index < Artifact.main.locations.size(); ++Index) {
      const std::optional<CDDebugLocation> &Location =
          Artifact.main.locations[Index];
      if (!Location)
        continue;
      OS << "  main " << Index << " = s" << Location->source << ":"
         << Location->line << ":" << Location->column << '\n';
    }
    for (unsigned FunctionIndex = 0;
         FunctionIndex < Artifact.functions.size(); ++FunctionIndex) {
      const CDBody &Body = Artifact.functions[FunctionIndex].body;
      for (unsigned Index = 0; Index < Body.locations.size(); ++Index) {
        const std::optional<CDDebugLocation> &Location = Body.locations[Index];
        if (!Location)
          continue;
        OS << "  function " << functionName(FunctionIndex) << " " << Index
           << " = s" << Location->source << ":" << Location->line << ":"
           << Location->column << '\n';
      }
    }
  }

  bool HasDebugRanges = false;
  for (const std::optional<CDDebugLocation> &Location : Artifact.main.locations)
    HasDebugRanges |= Location && Location->range.has_value();
  for (const CDFunction &Function : Artifact.functions)
    for (const std::optional<CDDebugLocation> &Location : Function.body.locations)
      HasDebugRanges |= Location && Location->range.has_value();

  if (HasDebugRanges) {
    OS << "\ndebug_ranges:\n";
    for (unsigned Index = 0; Index < Artifact.main.locations.size(); ++Index) {
      const std::optional<CDDebugLocation> &Location =
          Artifact.main.locations[Index];
      if (!Location || !Location->range)
        continue;
      const CDDebugRange &Range = *Location->range;
      OS << "  main " << Index << " = s" << Range.source << ":"
         << Range.start << ":" << Range.end << '\n';
    }
    for (unsigned FunctionIndex = 0;
         FunctionIndex < Artifact.functions.size(); ++FunctionIndex) {
      const CDBody &Body = Artifact.functions[FunctionIndex].body;
      for (unsigned Index = 0; Index < Body.locations.size(); ++Index) {
        const std::optional<CDDebugLocation> &Location = Body.locations[Index];
        if (!Location || !Location->range)
          continue;
        const CDDebugRange &Range = *Location->range;
        OS << "  function " << functionName(FunctionIndex) << " " << Index
           << " = s" << Range.source << ":" << Range.start << ":"
           << Range.end << '\n';
      }
    }
  }
}

} // namespace llvm::cd
