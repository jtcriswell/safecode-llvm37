; RUN: test.sh -e -t %t %s
; XFAIL: darwin, i386, i686

; Another example of bzero() writing out of bounds.
; This is in assembly because clang replaces calls to bzero() with
; llvm.memset().

target triple = "x86_64-unknown-linux-gnu"

define i32 @main() {
entry:
  %arr = alloca [100 x i8]
  %ptr = getelementptr [100 x i8]* %arr, i32 0, i32 50
  call void @bzero(i8* %ptr, i64 52)
  ret i32 0
}

declare void @bzero(i8*, i64)
