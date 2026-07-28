// RUN: %HAKC_RUN_ENFORCEMENT_PASS
// RUN: %HAKC_EVALUATE

#define __user __attribute__((kernel_user_ptr))
#define __percpu __attribute__((per_cpu_ptr))

struct data_struct {
  int a;
  int __user *p;
};

int bar(struct data_struct __percpu *);

// CHECK-LABEL: @HAKC_ORIG_foo(ptr noundef per_cpu_ptr %a) #0 section ".hakc.1.text"
int foo(struct data_struct __percpu *a) {
  if (a) {
// CHECK-LABEL: if.then
// CHECK: call ptr @check_hakc_data_access(ptr %1, i64 1, i64 65549)
// CHECK: call ptr @check_hakc_data_access(ptr %5, i64 1, i64 65549)
    (a->a)++;
// Kernel user pointers are not checked
// CHECK-NOT: call ptr @check_hakc_data_access
    int __user *p = a->p;
    (*p)++;
// CHECK: call i32 @bar(ptr noundef %11)
    return bar(a);
  }
  return 0;
}

// CHECK-LABEL: i32 @HAKC_XFER_foo(ptr noundef per_cpu_ptr %0) #0 section ".hakc.1.text"
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_percpu_to_clique(ptr %0, i64 16, i64 1, i32 13)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %2)
// CHECK: call void @hakc_color_address(ptr %0, i32 %1, i64 16)
