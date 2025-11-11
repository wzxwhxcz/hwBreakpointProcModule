#ifndef _HIDE_PROCFS_DIR_H_
#define _HIDE_PROCFS_DIR_H_

#include "ver_control.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/version.h>

static char g_hide_dir_name[256] = {0};
static int  g_hide_dir_len = 0;

/* 用真实的 ctx->actor 类型，避免版本差异（bool/int）带来的不匹配 */
typedef typeof(((struct dir_context *)0)->actor) filldir_fn_t;
static filldir_fn_t old_filldir;

/* 判断目标名是否需要隐藏 */
static inline bool should_hide(const char *name, int namelen)
{
    return (g_hide_dir_len > 0) &&
           (namelen == g_hide_dir_len) &&
           (strncmp(name, g_hide_dir_name, g_hide_dir_len) == 0);
}

/* 探测 filldir_t 是否返回 bool（某些内核是 bool，有些还是 int） */
#define FILLDIR_RETURNS_BOOL \
    __builtin_types_compatible_p( \
        typeof(((struct dir_context *)0)->actor), \
        bool (*)(struct dir_context *, const char *, int, loff_t, u64, unsigned int) \
    )

#if FILLDIR_RETURNS_BOOL
/* bool 版：返回 true 表示“已处理，可继续”；隐藏时返回 true 跳过该项并继续遍历 */
static bool my_filldir(struct dir_context *ctx,
                       const char *name, int namelen,
                       loff_t offset, u64 ino, unsigned int d_type)
{
    if (should_hide(name, namelen))
        return true;
    return old_filldir(ctx, name, namelen, offset, ino, d_type);
}
#else
/* int 版：返回 0 继续；隐藏时返回 0 跳过该项并继续遍历 */
static int my_filldir(struct dir_context *ctx,
                      const char *name, int namelen,
                      loff_t offset, u64 ino, unsigned int d_type)
{
    if (should_hide(name, namelen))
        return 0;
    return old_filldir(ctx, name, namelen, offset, ino, d_type);
}
#endif

/* kprobe 预处理：替换 ctx->actor 为我们的包装函数 */
static int handler_pre(struct kprobe *kp, struct pt_regs *regs)
{
#if defined(CONFIG_ARM64)
    struct dir_context *ctx = (struct dir_context *)regs->regs[1]; /* arg1 */
#elif defined(CONFIG_X86_64)
    struct dir_context *ctx = (struct dir_context *)regs->si;      /* arg1 */
#else
# error "Unsupported arch for kprobe argument extraction"
#endif
    if (!ctx || !ctx->actor)
        return 0;

    old_filldir = ctx->actor;
    ctx->actor  = my_filldir;
    return 0;
}

static struct kprobe kp_hide_procfs_dir = {
    .symbol_name = "proc_root_readdir",
    .pre_handler = handler_pre,
};

static bool start_hide_procfs_dir(const char *hide_dir_name)
{
    int ret;

    strlcpy(g_hide_dir_name, hide_dir_name ? hide_dir_name : "", sizeof(g_hide_dir_name));
    g_hide_dir_len = strlen(g_hide_dir_name);

    ret = register_kprobe(&kp_hide_procfs_dir);
    if (ret) {
        printk_debug("[hide_procfs_dir] register_kprobe failed: %d\n", ret);
        return false;
    }
    printk_debug("[hide_procfs_dir] kprobe installed, hiding \"%s\"\n", g_hide_dir_name);
    return true;
}

static void stop_hide_procfs_dir(void)
{
    unregister_kprobe(&kp_hide_procfs_dir);
    printk_debug("[hide_procfs_dir] kprobe removed\n");
}

#endif /* _HIDE_PROCFS_DIR_H_ */