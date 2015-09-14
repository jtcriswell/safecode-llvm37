; RUN: test.sh -p -t %t %s
; XFAIL: darwin, i386, i686

; Example of the correct usage of bzero().
; This is in assembly because clang replaces calls to bzero() with
; llvm.memset().

target triple = "x86_64-unknown-linux-gnu"

define i32 @main() {
entry:
  %arr = alloca [1 x i8]
  %ptr = getelementptr [1 x i8]* %arr, i32 0, i32 0
  store i8 1, i8* %ptr 
  call void @bzero(i8* %ptr, i64 1)
  %val = load i8* %ptr
  %val32 = sext i8 %val to i32
  ret i32 %val32
}

declare void @bzero(i8*, i64)
