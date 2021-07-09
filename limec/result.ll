
define i32 @main() {
entry:
  %a = alloca i32, align 4
  store i32 0, i32* %a, align 4
  store i32 5, i32* %a, align 4
  ret i32 0
}
