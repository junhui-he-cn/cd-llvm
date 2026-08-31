; RUN: llc -mtriple=cd-unknown-unknown -cd-artifact=module %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

define i32 @main() !dbg !5 {
entry:
  %bad = sdiv i32 1, 0, !dbg !10
  ret i32 %bad, !dbg !11
}

!cd.module = !{!40}
!40 = !{!"/workspace/cd-llvm-dependency.cd", !"dependency-runtime.cd", !"/workspace/cd-llvm-dependency.cd", i1 false}
!cd.sources = !{!20}
!20 = !{!"/workspace/cd-llvm-dependency.cd", !"dependency-runtime.cd", !"1 / 0;\0A"}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!1 = !DIFile(filename: "dependency-runtime.cd", directory: "")
!10 = !DILocation(line: 1, column: 1, scope: !5)
!11 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_sources:
; DIRECT: s0 module="/workspace/cd-llvm-dependency.cd" path="dependency-runtime.cd"
; DIRECT: debug_locations:
; DIRECT: function f0 3 = s0:1:1
; MACHINE: debug_sources:
; MACHINE: s0 module="/workspace/cd-llvm-dependency.cd" path="dependency-runtime.cd"
; MACHINE: debug_locations:
; MACHINE: function f0 3 = s0:1:1
