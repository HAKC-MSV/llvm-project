// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_ANALYSIS_SERVER
// RUN: %HAKC_RUN_ANALYSIS_PASS
// sleep for n seconds to let server finish constructing dag
// RUN: sleep 25
// RUN: %HAKC_RUN_ENFORCEMENT_PASS
// RUN: %HAKC_EVALUATE

#include <stdio.h>

char *GlobalString = "hello world";

// CHECK-LABEL: HAKC_ORIG_printstr
int printstr(char *str) {
  printf("%s %s\n", str, GlobalString);
  return 0;
}

// CHECK-LABEL: hakc_glob_init_xfer_GlobalString
// CHECK: call ptr @hakc_transfer_string(ptr @.str, i64 3, i64 13)

// CHECK-LABEL: HAKC_XFER_printstr
// CHECK: call ptr @hakc_transfer_string(ptr %0, i64 2, i64 13)
