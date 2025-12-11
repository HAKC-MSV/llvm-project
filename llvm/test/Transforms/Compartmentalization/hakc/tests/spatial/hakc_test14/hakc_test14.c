// RUN: %HAKC_RUN_ENFORCEMENT_PASS
// RUN: %HAKC_EVALUATE

#include <stdio.h>

char *GlobalString = "hello world";

// CHECK-LABEL: @HAKC_ORIG_printstr(ptr noundef %str) #0 section ".hakc.1.text"
int printstr(char *str) {
  // CHECK: call i32 @HAKC_VARF_printf_0(ptr noundef @.str.1, ptr noundef %0, ptr noundef %1)
  printf("%s %s\n", str, GlobalString);
  return 0;
}

// CHECK-LABEL: @HAKC_VARF_printf_0(ptr %0, ptr %1, ptr %2)
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_string(ptr %0, i64 2, i32 13)
// CHECK: call i32 @get_hakc_address_color(ptr %1)
// CHECK: call ptr @hakc_transfer_string(ptr %1, i64 2, i32 13)
// CHECK: call i32 @get_hakc_address_color(ptr %2)
// CHECK: call ptr @hakc_transfer_string(ptr %2, i64 2, i32 13)
// CHECK: call i32 (ptr, ...) @printf(ptr %4, ptr %6, ptr %8)
// CHECK: call void @hakc_color_address(ptr %2, i32 %7, i64 1)
// CHECK: call void @hakc_color_address(ptr %1, i32 %5, i64 1)
// CHECK: call void @hakc_color_address(ptr %0, i32 %3, i64 1)
// CHECK: ret i32 %9
