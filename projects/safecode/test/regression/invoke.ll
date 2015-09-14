; RUN: clang -emit-llvm -S -c -fmemsafety %s -o /dev/null
declare i64 @fwrite(i8*, i64, i64, i8*)

declare i32 @__gxx_personality_v0(...)

define i32 @main(i32 %argc, i8** %argv) uwtable {
entry:
  %0 = invoke i64 @fwrite(i8* null, i64 0, i64 0, i8* null)
                 to label %done unwind label %lpad

lpad:
  %1 = landingpad {i8*, i32} personality i32 (...)* @__gxx_personality_v0
          cleanup
  br label %done

done:
 ret i32 0

}

