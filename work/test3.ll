; ModuleID = 'test3.c'
source_filename = "test3.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.anon.3 = type { %struct.anon.4 }
%struct.anon.4 = type { i32 }
%struct.wow = type { %struct.atomic64_t, %struct.atomic64_t, %struct.anon, %struct.anon.0, %struct.anon.2 }
%struct.atomic64_t = type { i64 }
%struct.anon = type { %struct.kref, i32, %struct.atomic_t }
%struct.kref = type { %struct.refcount_struct }
%struct.refcount_struct = type { %struct.atomic_t }
%struct.atomic_t = type { i32 }
%struct.anon.0 = type { %struct.anon.1 }
%struct.anon.1 = type { %struct.refcount_struct }
%struct.anon.2 = type { i32 }
%struct.outside = type { %struct.inside }
%struct.inside = type { %struct.atomic_t, i32 }
%struct.test = type { %union.anon }
%union.anon = type { i64 }
%struct.testt = type { i16, [2 x i8] }

@st0 = dso_local global %struct.anon.3 zeroinitializer, align 4

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @foo() #0 {
  %1 = alloca %struct.wow, align 8
  %2 = alloca %struct.outside, align 4
  %3 = alloca %struct.test, align 8
  %4 = alloca %struct.testt, align 4
  ret void
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"uwtable", i32 1}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{!"clang version 14.0.6 (https://github.com/llvm/llvm-project.git f28c006a5895fc0e329fe15fead81e37457cb1d1)"}
