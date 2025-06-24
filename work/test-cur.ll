; ModuleID = 'test-cur.c'
source_filename = "test-cur.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.wow0 = type { %union.anon }
%union.anon = type { %struct.wow1 }
%struct.wow1 = type { i32, i32 }
%struct.anon = type { i32 }

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca %struct.wow0, align 4
  %2 = getelementptr inbounds %struct.wow0, %struct.wow0* %1, i32 0, i32 0
  %3 = bitcast %union.anon* %2 to %struct.anon*
  %4 = getelementptr inbounds %struct.anon, %struct.anon* %3, i32 0, i32 0
  store i32 3, i32* %4, align 4
  %5 = getelementptr inbounds %struct.wow0, %struct.wow0* %1, i32 0, i32 0
  %6 = bitcast %union.anon* %5 to %struct.wow1*
  %7 = getelementptr inbounds %struct.wow1, %struct.wow1* %6, i32 0, i32 0
  store i32 3, i32* %7, align 4
  ret i32 0
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"uwtable", i32 1}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{!"clang version 14.0.6 (https://github.com/llvm/llvm-project.git f28c006a5895fc0e329fe15fead81e37457cb1d1)"}
