#ifndef _HIDE_PROCFS_DIR_H_
#define _HIDE_PROCFS_DIR_H_

#include "ver_control.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/string.h>

static char g_hide_dir_name[256] = {0};
static int  g_hide_dir_len = 0;

/* 内核提供的回调类型，来自 linux/fs.h */
static filldir_t old_filldir;

/* 注意：filldir_t 的返回类型是 int，不是 bool */
static int my_filldir(struct dir_context *ctx,
                      const char *name,
                      int namelen,
                      loff_t offset,
                      u64 ino,
                      unsigned int d_type)
{
    /* 命中要隐藏的目录名时，返回 0 表示“跳过该项并继续遍历” */
    if (namelen == g_hide_dir_len &&
        g_hide_dir_len > 0 &&
        !strncmp(name, g_hide_dir_name, g_hide_dir_len)) {
        return 0;
    }
    /* 其余情况转发给原始回调，保持语义一致 */
    return old_filldir(ctx, name, namelen, offset, ino, d_type);
}

/* kprobe 预处理：把 ctx->actor 换成我们的包装函数，并保存原始回调 */
static int handler_pre(struct kprobe *kp, struct pt_regs *regs)
{
#if defined(CONFIG_ARM64)
    /* arm64: x1 是第二个参数 -> struct dir_context * */
    struct dir_context *ctx = (struct dir_context *)regs->regs[1];
#elif defined(CONFIG_X86_64)
    /* x86_64: 第二个参数在 RSI */
    struct dir_context *ctx = (struct dir_context *)regs->si;
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

static bool start_hide_procfs_dir(const char* hide_dir_name)
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

#endif  /* _HIDE_PROCFS_DIR_H_ */
