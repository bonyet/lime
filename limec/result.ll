
%Dog = type { i32 }

define i32 @main() {
entry:
  %dog = alloca %Dog, align 8
  ret i32 5
}
