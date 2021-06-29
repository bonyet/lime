
define i32 @sum(i32 %a_, i32 %b_) {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %loadtmp = load i32, i32* %a, align 4
  %loadtmp1 = load i32, i32* %b, align 4
  %0 = add i32 %loadtmp, %loadtmp1
  ret i32 %0
}

define i32 @main() {
entry:
  %a = alloca i32, align 4
  store i32 5, i32* %a, align 4
  %b = alloca i32, align 4
  store i32 10, i32* %b, align 4
  %c = alloca i32, align 4
  %loadtmp = load i32, i32* %a, align 4
  %loadtmp1 = load i32, i32* %b, align 4
  %calltmp = call i32 @sum(i32 %loadtmp, i32 %loadtmp1)
  store i32 %calltmp, i32* %c, align 4
  %d = alloca float, align 4
  store float 1.550000e+01, float* %d, align 4
  %loadtmp2 = load i32, i32* %a, align 4
  %cmptmp = icmp uge i32 %loadtmp2, 5
  br i1 %cmptmp, label %btrue, label %bfalse

btrue:                                            ; preds = %entry
  %f = alloca i32, align 4
  store i32 0, i32* %f, align 4
  br label %end

bfalse:                                           ; preds = %entry
  br label %end

end:                                              ; preds = %bfalse, %btrue
  %loadtmp3 = load i32, i32* %c, align 4
  ret i32 %loadtmp3
}
