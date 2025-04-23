// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE
#include <stdio.h>

char *GlobalString = "hello world";

int printstr(char *str) {
  printf("%s %s\n", str, GlobalString);
  return 0;
}

// CHECK_LABEL: HAKC_ORIG_printstr
