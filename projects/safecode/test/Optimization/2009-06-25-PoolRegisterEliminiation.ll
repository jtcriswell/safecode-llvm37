; ModuleID = 'const.bc'
target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:32:32"
target triple = "i386-pc-linux-gnu"

define i32 @main(i32 %argc, i8** %argv) nounwind {
entry:
	%"alloca point" = bitcast i32 0 to i32		; <i32> [#uses=0]
	%0 = mul i32 %argc, 4		; <i32> [#uses=1]
	%1 = call noalias i8* @malloc(i32 %0) nounwind		; <i8*> [#uses=1]
	%2 = bitcast i8* %1 to i32*		; <i32*> [#uses=1]
	%3 = sub i32 %argc, 5		; <i32> [#uses=1]
	%4 = getelementptr i32* %2, i32 %3		; <i32*> [#uses=1]
	%5 = load i32* %4, align 1		; <i32> [#uses=1]
	br label %return

return:		; preds = %entry
	ret i32 %5
}

declare noalias i8* @malloc(i32) nounwind
