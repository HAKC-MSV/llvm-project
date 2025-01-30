// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
// RUN: %HAKC_EVALUATE

struct data_struct {
    int a;
};

int bar(struct data_struct *);

int foo(struct data_struct *a) {
    if (a) {
        (a->a)++;
        return bar(a);
    }
    return 0;
}

// this output is probably wrong (not seeing the pointer being checked)
// CHECK: %10 = getelementptr inbounds %struct.data_struct, %struct.data_struct* %9, i64 0, i32 0
