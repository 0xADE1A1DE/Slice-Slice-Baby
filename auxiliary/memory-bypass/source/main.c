#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stephan van Schaik");
MODULE_DESCRIPTION("Bypass /dev/mem checks.");
MODULE_VERSION("1.0");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
static struct kprobe kallsyms_probe = {
    .symbol_name = "kallsyms_lookup_name",
};

typedef unsigned long (*kallsyms_lookup_name_t)(const char *);
kallsyms_lookup_name_t my_kallsyms_lookup_name = NULL;
#endif

int no_pud_huge(pud_t pud)
{
    return 0;
}

int no_pmd_huge(pmd_t pmd)
{
    return 0;
}

typedef int (*pud_huge_t)(pud_t);
typedef int (*pmd_huge_t)(pmd_t);

pud_huge_t pud_huge = no_pud_huge;
pmd_huge_t pmd_huge = no_pmd_huge;

#define IOCTL_MAGIC 0xf4

#define IOCTL_READ_PTE _IOWR(IOCTL_MAGIC, 1, struct memkit_read_pte)
#define IOCTL_WRITE_PTE _IOW(IOCTL_MAGIC, 2, struct memkit_write_pte)

struct memkit_read_pte
{
    u64 pte;
    u64 ptr;
    pid_t pid;
};

struct memkit_write_pte
{
    u64 pte;
    u64 ptr;
    pid_t pid;
};

static struct mm_struct *get_mm(pid_t pid)
{
    struct task_struct *task;
    struct pid *vpid;

    task = current;

    if (pid != 0)
    {
        vpid = find_vpid(pid);

        if (!vpid)
            return NULL;

        task = pid_task(vpid, PIDTYPE_PID);

        if (!task)
            return NULL;
    }

    if (task->mm)
    {
        return task->mm;
    }

    return task->active_mm;
}

static int bypass(struct kretprobe_instance *probe, struct pt_regs *regs)
{
#if defined(CONFIG_X86_64)
    regs->ax = 1;
#elif defined(CONFIG_ARM64)
    regs->regs[0] = 1;
#else
#error unsupported platform
#endif

    return 0;
}

static struct kretprobe probe = {
    .handler = bypass,
    .maxactive = 20,
};

static int memkit_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int memkit_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static int memkit_read_pte(struct file *filp, unsigned long arg)
{
    struct memkit_read_pte params;
    struct mm_struct *mm;
    pgd_t *pgd;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d_t *p4d;
#endif
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    int ret = 0;

    ret = copy_from_user(&params, (const void *)arg, sizeof params);

    if (ret < 0)
    {
        return ret;
    }

    mm = get_mm(params.pid);

    if (mm == NULL)
    {
        return -ENOENT;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    down_read(&mm->mmap_lock);
#else
    down_read(&mm->mmap_sem);
#endif
    spin_lock(&mm->page_table_lock);

    pgd = pgd_offset(mm, params.ptr);

    if (pgd_none(*pgd) || pgd_bad(*pgd))
    {
        goto err_unlock;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d = p4d_offset(pgd, params.ptr);

    if (p4d_none(*p4d) || p4d_bad(*p4d))
    {
        goto err_unlock;
    }

    pud = pud_offset(p4d, params.ptr);
#else
    pud = pud_offset(pgd, params.ptr);
#endif

    if (pud_none(*pud) || pud_bad(*pud))
    {
        goto err_unlock;
    }

    if (pud_huge(*pud))
    {
        params.pte = pud_val(*pud);
        goto finish;
    }

    pmd = pmd_offset(pud, params.ptr);

    if (pmd_none(*pmd) || pmd_bad(*pmd))
    {
        goto err_unlock;
    }

    if (pmd_huge(*pmd))
    {
        params.pte = pmd_val(*pmd);
        goto finish;
    }

    pte = pte_offset_kernel(pmd, params.ptr);
    params.pte = pte_val(*pte);
    pte_unmap(pte);

finish:
    spin_unlock(&mm->page_table_lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    up_read(&mm->mmap_lock);
#else
    up_read(&mm->mmap_sem);
#endif

    ret = copy_to_user((void *)arg, &params, sizeof params);

    if (ret < 0)
    {
        return ret;
    }

    return ret;

err_unlock:
    spin_unlock(&mm->page_table_lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    up_read(&mm->mmap_lock);
#else
    up_read(&mm->mmap_sem);
#endif
    return ret;
}

static int memkit_write_pte(struct file *filp, unsigned long arg)
{
    struct memkit_write_pte params;
    struct mm_struct *mm;
    pgd_t *pgd;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d_t *p4d;
#endif
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    int ret = 0;

    ret = copy_from_user(&params, (const void *)arg, sizeof params);

    if (ret < 0)
    {
        return ret;
    }

    mm = get_mm(params.pid);

    if (mm == NULL)
    {
        return -ENOENT;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    down_read(&mm->mmap_lock);
#else
    down_read(&mm->mmap_sem);
#endif
    spin_lock(&mm->page_table_lock);

    pgd = pgd_offset(mm, params.ptr);

    if (pgd_none(*pgd) || pgd_bad(*pgd))
    {
        goto err_unlock;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d = p4d_offset(pgd, params.ptr);

    if (p4d_none(*p4d) || p4d_bad(*p4d))
    {
        goto err_unlock;
    }

    pud = pud_offset(p4d, params.ptr);
#else
    pud = pud_offset(pgd, params.ptr);
#endif

    if (pud_none(*pud) || pud_bad(*pud))
    {
        goto err_unlock;
    }

    pmd = pmd_offset(pud, params.ptr);

    if (pmd_none(*pmd) || pmd_bad(*pmd))
    {
        goto err_unlock;
    }

    pte = pte_offset_kernel(pmd, params.ptr);

    if (!pte_none(*pte))
    {
#if defined(CONFIG_X86_64)
        asm volatile("invlpg (%0)" ::"r"(params.ptr) : "memory");
#elif defined(CONFIG_ARM64)
        // TODO: invalidate the page.
#else
#error unsupported platform
#endif
    }

    set_pte(pte, __pte(params.pte));
    pte_unmap(pte);

    spin_unlock(&mm->page_table_lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    up_read(&mm->mmap_lock);
#else
    up_read(&mm->mmap_sem);
#endif

    ret = copy_to_user((void *)arg, &params, sizeof params);

    if (ret < 0)
    {
        return ret;
    }

    return ret;

err_unlock:
    spin_unlock(&mm->page_table_lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    up_read(&mm->mmap_lock);
#else
    up_read(&mm->mmap_sem);
#endif
    return ret;
}

static long memkit_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
    case IOCTL_READ_PTE:
        return memkit_read_pte(filp, arg);
    case IOCTL_WRITE_PTE:
        return memkit_write_pte(filp, arg);
    default:
        return -EINVAL;
    }
}

static struct class *cls = NULL;
static dev_t devno = 0;
static struct device *dev = NULL;
static struct cdev cdev = {};
static struct file_operations file_ops = {
    .owner = THIS_MODULE,
    .read = NULL,
    .write = NULL,
    .open = memkit_open,
    .unlocked_ioctl = memkit_ioctl,
    .release = memkit_release,
};

static int __init mem_bypass_init(void)
{
    int ret = 0;

    ret = register_kprobe(&kallsyms_probe);

    if (ret < 0)
    {
        printk(KERN_INFO "memkit: symbol \"kallsyms_lookup_name\" not found");
        return ret;
    }

    my_kallsyms_lookup_name = (kallsyms_lookup_name_t)kallsyms_probe.addr;
    unregister_kprobe(&kallsyms_probe);

    pud_huge = (pud_huge_t)my_kallsyms_lookup_name("pud_huge");

    if (!pud_huge)
    {
        printk(KERN_INFO "memkit: symbol \"pud_huge\" not found\n");
        pud_huge = no_pud_huge;
    }

    pmd_huge = (pmd_huge_t)my_kallsyms_lookup_name("pmd_huge");

    if (!pmd_huge)
    {
        printk(KERN_INFO "memkit: symbol \"pmd_huge\" not found\n");
        pmd_huge = no_pmd_huge;
    }

    probe.kp.symbol_name = "devmem_is_allowed";
    ret = register_kretprobe(&probe);

    if (ret < 0)
    {
        return ret;
    }

    ret = alloc_chrdev_region(&devno, 0, 1, "memkit");

    if (ret < 0)
    {
        goto err_unregister_kretprobe;
    }

    cdev_init(&cdev, &file_ops);
    cdev.owner = THIS_MODULE;

    ret = cdev_add(&cdev, devno, 1);

    if (ret < 0)
    {
        goto err_unregister_region;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    struct lock_class_key key = {};
    cls = __class_create(THIS_MODULE, "memkit", &key);
#else
    cls = class_create("memkit");
#endif

    dev = device_create(cls, NULL, devno, NULL, "memkit");

    pr_info("memkit: ready");

    return 0;

err_unregister_region:
    unregister_chrdev_region(0, 1);
err_unregister_kretprobe:
    unregister_kretprobe(&probe);
    return ret;
}

static void __exit mem_bypass_exit(void)
{
    device_destroy(cls, devno);
    class_destroy(cls);
    cdev_del(&cdev);
    unregister_chrdev_region(devno, 1);
    unregister_kretprobe(&probe);
}

module_init(mem_bypass_init);
module_exit(mem_bypass_exit);
