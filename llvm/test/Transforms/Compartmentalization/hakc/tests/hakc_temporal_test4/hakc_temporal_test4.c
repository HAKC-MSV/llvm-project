// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

struct struct_a {
  int a;
};

void init(struct struct_a *a){
  a->a = 0;
}

void foo(struct struct_a *a){}
void bar(struct struct_a *a){}

void read0(struct struct_a *a){ int b = a->a; foo(a);}
void read1(struct struct_a *a){ int b = a->a; foo(a);}

void read_write0(struct struct_a *a){ a->a += 53; bar(a);}
void read_write1(struct struct_a *a){ a->a += 53; bar(a);}

int main(){
  struct struct_a *a;
  a->a+=0;

  init(a);
  read0(a);
  read1(a);

  read_write0(a);
  read_write1(a);

  return 0;
}

/*
void foo(struct struct_a *a) {
    // execute only
    int (*fptr)(struct struct_a*);
    fptr = &bar;
}
*/

// CHECK: NULL

