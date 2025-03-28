// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

#define __user __attribute__((kernel_user_ptr))
#define __percpu __attribute__((per_cpu_ptr))

struct data_struct {
  int a;
  int __user *p;
};

int bar(struct data_struct __percpu *);

// CHECK-LABEL: @HAKC_ORIG_foo(ptr noundef per_cpu_ptr %a) #0 section ".hakc.2.text"
int foo(struct data_struct __percpu *a) {
  if (a) {
// CHECK-LABEL: if.then
// CHECK: call ptr @check_hakc_data_access(ptr %1, i64 2, i64 139264)
    (a->a)++;
    int __user *p = a->p;
    (*p)++;
// CHECK: call i32 @bar(ptr noundef %5)
    return bar(a);
  }
  return 0;
}

// CHECK-LABEL: i32 @HAKC_XFER_foo(ptr noundef per_cpu_ptr %0) #0 section ".hakc.2.text"
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: %2 = call ptr @hakc_transfer_percpu_to_clique(ptr %0, i64 16, i64 2, i64 13), !dbg !29
// CHECK: call i32 @HAKC_ORIG_foo(ptr %2)
// CHECK: call void @hakc_color_address(ptr %0, i32 %1, i64 16)
