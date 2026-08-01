//===- CDBytecodeFormatTest.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CDBytecodeFormat.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "llvm/Support/raw_ostream.h"

#include <limits>
#include <string>

using ::testing::HasSubstr;

using namespace llvm::cd;

namespace {

CDArtifact minimalArtifact() {
  CDArtifact Artifact;
  Artifact.main.registers = 1;
  Artifact.main.instructions.push_back(
      CDInstruction::returnValue(/*Value=*/0));
  return Artifact;
}

void expectValidationError(const CDArtifact &Artifact,
                           const char *ExpectedFragment) {
  std::string Error;
  ASSERT_FALSE(validateArtifact(Artifact, Error));
  EXPECT_THAT(Error, HasSubstr(ExpectedFragment));
}

TEST(CDBytecodeFormatTest, AcceptsMainJumpTargetEqualToInstructionCount) {
  CDArtifact Artifact = minimalArtifact();
  Artifact.main.instructions = {
      CDInstruction::jump(/*Target=*/2),
      CDInstruction::returnValue(/*Value=*/0),
  };

  std::string Error;
  EXPECT_TRUE(validateArtifact(Artifact, Error)) << Error;
}

TEST(CDBytecodeFormatTest, RejectsMainJumpTargetPastInstructionCount) {
  CDArtifact Artifact = minimalArtifact();
  Artifact.main.instructions = {
      CDInstruction::jump(/*Target=*/3),
      CDInstruction::returnValue(/*Value=*/0),
  };

  expectValidationError(Artifact, "jump target");
}

TEST(CDBytecodeFormatTest, RejectsRegisterReferenceOutsideBody) {
  CDArtifact Artifact = minimalArtifact();
  Artifact.main.instructions = {
      CDInstruction::move(/*Destination=*/1, /*Source=*/0),
      CDInstruction::returnValue(/*Value=*/0),
  };

  expectValidationError(Artifact, "register");
}

TEST(CDBytecodeFormatTest, RejectsNonFiniteNumberConstant) {
  CDArtifact Artifact = minimalArtifact();
  Artifact.constants.push_back(
      CDConstant::number(std::numeric_limits<double>::infinity()));

  expectValidationError(Artifact, "finite number");
}

TEST(CDBytecodeFormatTest, RejectsInvalidStringConstant) {
  CDArtifact Artifact = minimalArtifact();
  Artifact.constants.push_back(CDConstant::string(std::string("\xff", 1)));

  expectValidationError(Artifact, "valid UTF-8");
}

TEST(CDBytecodeFormatTest, RejectsUnexpectedInstructionFields) {
  CDArtifact Artifact = minimalArtifact();
  CDInstruction Return = CDInstruction::returnValue(/*Value=*/0);
  Return.reference = 0;
  Artifact.main.instructions = {Return};

  expectValidationError(Artifact, "unexpected table reference");
}

TEST(CDBytecodeFormatTest, RejectsOutOfRangeTableReferences) {
  CDArtifact Artifact = minimalArtifact();

  CDInstruction Constant = CDInstruction::constant(/*Destination=*/0,
                                                    /*Constant=*/0);
  Artifact.main.instructions = {Constant};
  expectValidationError(Artifact, "constant reference");

  CDInstruction Load;
  Load.opcode = CDOpcode::LoadVar;
  Load.result = 0;
  Load.reference = 0;
  Artifact.main.instructions = {Load};
  expectValidationError(Artifact, "name reference");

  CDInstruction MakeFunction =
      CDInstruction::makeFunction(/*Destination=*/0, /*Function=*/0);
  Artifact.main.instructions = {MakeFunction};
  expectValidationError(Artifact, "function reference");
}

TEST(CDBytecodeFormatTest, RejectsMainParametersAndFunctionArityMismatch) {
  CDArtifact Artifact = minimalArtifact();
  Artifact.main.parameterNames.push_back("value");
  expectValidationError(Artifact, "main body");

  Artifact.main.parameterNames.clear();
  CDFunction Function;
  Function.arity = 1;
  Artifact.functions.push_back(Function);
  expectValidationError(Artifact, "arity");
}

TEST(CDBytecodeFormatTest, RejectsWrongInstructionShape) {
  CDArtifact Artifact = minimalArtifact();
  CDInstruction Return = CDInstruction::returnValue(/*Value=*/0);
  Return.operands.push_back(0);
  Artifact.main.instructions = {Return};

  expectValidationError(Artifact, "operand count");
}

TEST(CDBytecodeFormatTest, SerializesCanonicalSectionsAndEscapes) {
  CDArtifact Artifact = minimalArtifact();
  CDConstant Number;
  Number.kind = CDConstant::Number;
  Number.text = "1.0";
  Artifact.constants = {Number, CDConstant::boolean(true),
                        CDConstant::string("hello\n\"")};
  Artifact.names = {"a\n\""};
  Artifact.main.instructions = {
      CDInstruction::constant(/*Destination=*/0, /*Constant=*/0),
      CDInstruction::print(/*Value=*/0),
      CDInstruction::returnValue(/*Value=*/0),
  };

  std::string Output;
  llvm::raw_string_ostream Stream(Output);
  serializeArtifact(Artifact, Stream);
  Stream.flush();

  EXPECT_EQ(Output,
            "cdbc 0.1\n\n"
            "constants:\n"
            "  c0 = number 1\n"
            "  c1 = bool true\n"
            "  c2 = string \"hello\\n\\\"\"\n\n"
            "names:\n"
            "  n0 = \"a\\n\\\"\"\n\n"
            "main registers=1:\n"
            "  r0 = constant c0\n"
            "  print r0\n"
            "  return r0\n");
}

} // namespace
