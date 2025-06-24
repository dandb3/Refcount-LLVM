; ModuleID = 'test.c'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.anon = type { %struct.anon.2 }
%struct.anon.2 = type { %union.anon.3 }
%union.anon.3 = type { %struct.atomic64_t }
%struct.atomic64_t = type { i64 }
%union.anon.4 = type { %struct.wow6 }
%struct.wow6 = type { %struct.atomic_t, %struct.atomic_t, %struct.atomic_t, %struct.atomic_t, %struct.atomic64_t, %struct.kref }
%struct.atomic_t = type { i32 }
%struct.kref = type { %struct.refcount_struct }
%struct.refcount_struct = type { %struct.atomic_t }
%struct.wow = type { %union.anon }
%union.anon = type { i32 }
%struct.wow1 = type { %union.anon.0, %union.anon.0 }
%union.anon.0 = type { %struct.refcount_struct }
%struct.wow2 = type { %struct.refcount_struct }
%struct.wow3 = type { %struct.wow2, %struct.refcount_struct }
%struct.wow6.1 = type { i32 }

@st1 = dso_local global %struct.anon zeroinitializer, align 8
@st2 = dso_local global %union.anon.4 zeroinitializer, align 8

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @func() #0 {
  %1 = alloca %struct.wow, align 4
  %2 = alloca %struct.wow1, align 4
  %3 = alloca %struct.wow2, align 4
  %4 = alloca %struct.wow3, align 4
  %5 = alloca %struct.wow6, align 8
  %6 = alloca %struct.wow6.1, align 4
  ret i32 0
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"uwtable", i32 1}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{!"clang version 14.0.6 (https://github.com/llvm/llvm-project.git f28c006a5895fc0e329fe15fead81e37457cb1d1)"}
