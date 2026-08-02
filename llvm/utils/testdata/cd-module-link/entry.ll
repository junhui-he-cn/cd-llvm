declare void @cd_print(double)

define i32 @main() {
entry:
  call void @cd_print(double 1.0)
  call void @cd_print(double 3.0)
  ret i32 0
}

!cd.module = !{!0}
!0 = !{!"/workspace/cd-llvm-entry.cd", !"entry.cd", !"/workspace/cd-llvm-entry.cd", i1 true, i64 0}
!cd.dependencies = !{!1}
!1 = !{!"import", !"/workspace/cd-llvm-dependency.cd", i64 2, !"./dependency.cd"}
