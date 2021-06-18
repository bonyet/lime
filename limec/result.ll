
%Dog = type { i32 }

define i32 @main() {
entry:
  %dog = alloca %Dog, align 8
  %geptmp = getelementptr %Dog, %Dog* %dog, i32 0, i32 0
  store i32 5, i32* %geptmp, align 4
  ret i32 5
}
