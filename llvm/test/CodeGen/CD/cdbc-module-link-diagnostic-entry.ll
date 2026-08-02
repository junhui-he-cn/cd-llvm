; RUN: llc -mtriple=cd-unknown-unknown -cd-artifact=module %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare void @cd_print(double)

define i32 @main() !dbg !5 {
entry:
  call void @cd_print(double 1.0), !dbg !10
  call void @cd_print(double 3.0), !dbg !11
  ret i32 0, !dbg !12
}

!cd.module = !{!40}
!40 = !{!"/workspace/cd-llvm-entry.cd", !"entry-runtime.cd", !"/workspace/cd-llvm-entry.cd", i1 true, i64 0}
!cd.dependencies = !{!41}
!41 = !{!"import", !"/workspace/cd-llvm-dependency.cd", i64 2, !"./dependency-runtime.cd"}
!cd.sources = !{!20}
!20 = !{!"/workspace/cd-llvm-entry.cd", !"entry-runtime.cd", !"print 1;\0Aprint 3;\0A"}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!1 = !DIFile(filename: "entry-runtime.cd", directory: "")
!10 = !DILocation(line: 1, column: 1, scope: !5)
!11 = !DILocation(line: 2, column: 1, scope: !5)
!12 = !DILocation(line: 2, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_sources:
; DIRECT: s0 module="/workspace/cd-llvm-entry.cd" path="entry-runtime.cd"
; DIRECT: debug_locations:
; DIRECT: main 1 = s0:1:1
; DIRECT: main 3 = s0:2:1
; DIRECT: main 5 = s0:2:1
; MACHINE: debug_sources:
; MACHINE: s0 module="/workspace/cd-llvm-entry.cd" path="entry-runtime.cd"
; MACHINE: debug_locations:
; MACHINE: main 1 = s0:1:1
; MACHINE: main 3 = s0:2:1
; MACHINE: main 5 = s0:2:1
