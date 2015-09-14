; RUN: clang -emit-llvm -S -c -fmemsafety %s -o %t
declare i64 @fwrite(i8*, i64, i64, i8*)
declare i64 @printf(i8*, ...)

declare i32 @__gxx_personality_v0(...)

define i32 @main(i32 %argc, i8** %argv) uwtable {
entry:
  %0 = invoke i64 @fwrite(i8* null, i64 0, i64 0, i8* null)
                 to label %cont unwind label %lpad

cont:
  %1 = invoke i64 (i8*, ...)* @printf(i8* null, i64 0)
                 to label %done unwind label %lpad

lpad:
  %val = phi i32 [1, %entry], [2, %cont]
  %2 = landingpad {i8*, i32} personality i32 (...)* @__gxx_personality_v0
          cleanup
  br label %done

done:
  %retval= phi i32 [%val, %lpad], [0, %cont]
  ret i32 %retval
}

