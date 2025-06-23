//
// Created by de29664 on 6/17/25.
//

// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

// Adapted from init/main.c in the Linux kernel and related sources
#define __cold
#ifndef __latent_entropy
# define __latent_entropy
#endif
#define __noinitretpoline

#define __section(section)              __attribute__((__section__(section)))

#define __init		__section(".init.text") __cold  __latent_entropy __noinitretpoline

#define __initdata	__section(".init.data")

#define NULL ((void *)0)

typedef short s16;
typedef unsigned short u16;
typedef char s8;
typedef unsigned char u8;

typedef int (*initcall_t)(void);
typedef initcall_t initcall_entry_t;

extern initcall_entry_t __initcall_start[];
extern initcall_entry_t __initcall0_start[];
extern initcall_entry_t __initcall1_start[];
extern initcall_entry_t __initcall2_start[];
extern initcall_entry_t __initcall3_start[];
extern initcall_entry_t __initcall4_start[];
extern initcall_entry_t __initcall5_start[];
extern initcall_entry_t __initcall6_start[];
extern initcall_entry_t __initcall7_start[];
extern initcall_entry_t __initcall_end[];


static const char *initcall_level_names[] __initdata = {
  "pure",
  "core",
  "postcore",
  "arch",
  "subsys",
  "fs",
  "device",
  "late",
};

static initcall_entry_t *initcall_levels[] __initdata = {
  __initcall0_start,
  __initcall1_start,
  __initcall2_start,
  __initcall3_start,
  __initcall4_start,
  __initcall5_start,
  __initcall6_start,
  __initcall7_start,
  __initcall_end,
};

struct kernel_param;
struct module;

struct kernel_param_ops {
  /* How the ops should behave */
  unsigned int flags;
  /* Returns 0, or -errno.  arg is in kp->arg. */
  int (*set)(const char *val, const struct kernel_param *kp);
  /* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
  int (*get)(char *buffer, const struct kernel_param *kp);
  /* Optional function to free kp->arg when module unloaded. */
  void (*free)(void *arg);
};

struct kparam_string {
  unsigned int maxlen;
  char *string;
};

struct kparam_array
{
  unsigned int max;
  unsigned int elemsize;
  unsigned int *num;
  const struct kernel_param_ops *ops;
  void *elem;
};

struct kernel_param {
  const char *name;
  struct module *mod;
  const struct kernel_param_ops *ops;
  const u16 perm;
  s8 level;
  u8 flags;
  union {
    void *arg;
    const struct kparam_string *str;
    const struct kparam_array *arr;
  };
};


extern char *parse_args(const char *name,
                      char *args,
                      const struct kernel_param *params,
                      unsigned num,
                      s16 level_min,
                      s16 level_max,
                      void *arg,
                      int (*unknown)(char *param, char *val,
                                     const char *doing, void *arg));
extern int do_one_initcall(initcall_t fn);

extern const struct kernel_param __start___param[], __stop___param[];

void trace_initcall_level(const char *level_name);

static inline initcall_t initcall_from_entry(initcall_entry_t *entry)
{
  return *entry;
}

static int __init ignore_unknown_bootoption(char *param, char *val,
                               const char *unused, void *arg)
{
  return 0;
}

// CHECK-LABEL: HAKC_ORIG_do_initcall_level
void __init do_initcall_level(int level, char *command_line)
{
  initcall_entry_t *fn;

  parse_args(initcall_level_names[level],
             command_line, __start___param,
             __stop___param - __start___param,
             level, level,
             NULL, ignore_unknown_bootoption);

  trace_initcall_level(initcall_level_names[level]);
  for (fn = initcall_levels[level]; fn < initcall_levels[level+1]; fn++)
    do_one_initcall(initcall_from_entry(fn));
}
