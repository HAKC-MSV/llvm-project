// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_ANALYSIS_SERVER
// RUN: %HAKC_RUN_ANALYSIS_PASS
// RUN: %HAKC_EVALUATE

struct data_struct {
  int a;
};

int bar(struct data_struct *);

// CHECK: @foo

int foo(struct data_struct *a) {
  if (a) {
    (a->a)++;
    return bar(a);
  }
  return 0;
}
