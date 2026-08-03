; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare void @cd_print(double)

define i32 @main() {
entry:
  %result = call double @identity(double 2.0), !dbg !10
  call void @cd_print(double %result), !dbg !11
  ret i32 0, !dbg !12
}

define double @identity(double %input) !dbg !6 {
entry:
  %result = fadd double %input, 1.0, !dbg !13
  %bad = fdiv double %result, 0.0, !dbg !16
  ret double %bad, !dbg !14
}

!cd.sources = !{!20}
!20 = !{!"contract.cd", !"fun identity(input) { return (input + 1) / 0; }\0Aprint(identity(2));\0A"}
!cd.ranges = !{!43, !44}
!43 = !{!13, i64 0, i64 1}
!44 = !{!14, i64 20, i64 21}

!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "contract.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 2,
    type: !3, unit: !0, retainedNodes: !2)
!6 = distinct !DISubprogram(name: "identity", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 2, column: 1, scope: !5)
!11 = !DILocation(line: 2, column: 1, scope: !5)
!12 = !DILocation(line: 2, column: 1, scope: !5)
!13 = !DILocation(line: 1, column: 29, scope: !6)
!14 = !DILocation(line: 1, column: 1, scope: !6)
!16 = !DILocation(line: 1, column: 42, scope: !6)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_ranges:
; DIRECT: function f0
; MACHINE: debug_ranges:
; MACHINE: function f0
