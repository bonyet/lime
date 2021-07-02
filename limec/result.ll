
define i32 @main() {
entry:
  br i1 false, label %btrue, label %bfalse

btrue:                                            ; preds = %entry
  ret i32 5

bfalse:                                           ; preds = %entry
  br label %end

end:                                              ; preds = %bfalse
  br i1 true, label %btrue1, label %bfalse2

btrue1:                                           ; preds = %end
  ret i32 10

bfalse2:                                          ; preds = %end
  br label %end3

end3:                                             ; preds = %bfalse2
  ret i32 5
}
