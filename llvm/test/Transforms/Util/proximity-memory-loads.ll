; RUN: opt -disable-output -passes=print-proximity-memory-loads -max-proximity=13 %s 2>&1 | FileCheck %s

; CHECK: {{^}}Pair of memory loads are within proximity:
; CHECK: %3 = load i32, ptr %2, align 4
; CHECK: and
; CHECK: %5 = load i32, ptr %4, align 4
define i32 @foo(ptr %0) {
  %2 = getelementptr i32, ptr %0, i64 4
  %3 = load i32, ptr %2, align 4 
  %4 = getelementptr i32, ptr %0, i64 8
  %5 = load i32, ptr %4, align 4
  %6 = add i32 %5, %3
  ret i32 %6
}

; CHECK: {{^}}Pair of memory loads are within proximity:
; CHECK: %1 = load i32, ptr inttoptr (i64 272 to ptr), align 16
; CHECK: and
; CHECK: %2 = load i32, ptr inttoptr (i64 289 to ptr), align 4
define i32 @bar() {
  %1 = load i32, ptr inttoptr (i64 272 to ptr), align 16
  %2 = load i32, ptr inttoptr (i64 289 to ptr), align 4
  %3 = add i32 %2, %1
  ret i32 %3
}
