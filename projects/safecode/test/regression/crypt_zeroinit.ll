; RUN: clang -c -fmemsafety %s -o /dev/null
; ModuleID = 'test.c'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.crypt_data = type { [128 x i8], [32768 x i8], [32768 x i8], [32768 x i8], [32768 x i8], [14 x i8], [2 x i8], i64, i32, i32 }

@.str = private unnamed_addr constant [7 x i8] c"passwd\00", align 1
@.str1 = private unnamed_addr constant [5 x i8] c"hash\00", align 1

define i32 @main() nounwind uwtable {
entry:
  %retval = alloca i32, align 4
  %buffer = alloca %struct.crypt_data, align 8
  store i32 0, i32* %retval
  %call = call i8* @crypt_r(i8* getelementptr inbounds ([7 x i8]* @.str, i32 0, i32 0), i8* getelementptr inbounds ([5 x i8]* @.str1, i32 0, i32 0), %struct.crypt_data* %buffer) nounwind
  ret i32 0
}

declare i8* @crypt_r(i8*, i8*, %struct.crypt_data*) nounwind
