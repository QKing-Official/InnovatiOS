#include <kernel/proc/scheduler.h>

#include <kernel/arch/x86_64/gdt.h>
#include <kernel/assets/innovatios_logo.h>
#include <kernel/drivers/framebuffer.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/drivers/pit.h>
#include <kernel/drivers/serial.h>
#include <kernel/lib/string.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/vmm.h>

#define TASK_COUNT 2
#define USER_CODE_BASE  0x0000000000400000ULL
#define USER_STACK_BASE 0x0000000000600000ULL
#define TASK_SLOT_STRIDE 0x0000000000020000ULL

typedef struct process {
    bool used;
    u64 pid;
    const char *name;
    u64 code_phys;
    u64 stack_phys;
    u64 code_vaddr;
    u64 stack_vaddr;
    interrupt_frame_t frame;
} process_t;

static const u8 demo_spin_code[] = {
    0xF3, 0x90,
    0xEB, 0xFC
};

static process_t g_tasks[TASK_COUNT];
static u64 g_hhdm_offset;
static u64 g_task_count;
static u64 g_current_task;
static u64 g_next_pid = 1;
static u64 g_switch_count;
static bool g_scheduler_running;
static i64 g_anim_x;
static i64 g_anim_y;
static i64 g_anim_dx;
static i64 g_anim_dy;
static u64 g_anim_max_x;
static u64 g_anim_max_y;
static u64 g_last_tick_log;
static const u64 g_logo_size = 4;

static void *phys_to_virt(u64 phys) {
    return (void *)(phys + g_hhdm_offset);
}

static void scheduler_fatal(const char *message) {
    serial_write(message);
    serial_write("\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void init_frame(interrupt_frame_t *frame, u64 rip, u64 rsp) {
    k_memset(frame, 0, sizeof(*frame));
    frame->rip = rip;
    frame->cs = GDT_USER_CODE | 3u;
    frame->rflags = 0x202;
    frame->rsp = rsp;
    frame->ss = GDT_USER_DATA | 3u;
}

static void setup_task(process_t *task, const char *name, u64 slot) {
    task->code_phys = pmm_alloc_page();
    task->stack_phys = pmm_alloc_page();
    if (task->code_phys == 0 || task->stack_phys == 0) {
        scheduler_fatal("scheduler: failed to allocate demo task pages");
    }

    task->code_vaddr = USER_CODE_BASE + (slot * TASK_SLOT_STRIDE);
    task->stack_vaddr = USER_STACK_BASE + (slot * TASK_SLOT_STRIDE);
    task->name = name;
    task->pid = g_next_pid++;
    task->used = true;

    if (vmm_map(vmm_kernel_space(), task->code_vaddr, task->code_phys,
                VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER) != 0) {
        scheduler_fatal("scheduler: failed to map demo code page");
    }

    if (vmm_map(vmm_kernel_space(), task->stack_vaddr, task->stack_phys,
                VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER) != 0) {
        scheduler_fatal("scheduler: failed to map demo stack page");
    }

    k_memcpy(phys_to_virt(task->code_phys), demo_spin_code, sizeof(demo_spin_code));
    init_frame(&task->frame, task->code_vaddr, task->stack_vaddr + VMM_PAGE_SIZE);
}

#if CONFIG_FRAMEBUFFER
static void update_demo_overlay(void) {
    if (fb_width() == 0 || fb_height() == 0) {
        return;
    }

    g_anim_x += g_anim_dx;
    g_anim_y += g_anim_dy;

    if (g_anim_x <= 0) {
        g_anim_x = 0;
        g_anim_dx = -g_anim_dx;
    } else if ((u64)g_anim_x >= g_anim_max_x) {
        g_anim_x = (i64)g_anim_max_x;
        g_anim_dx = -g_anim_dx;
    }

    if (g_anim_y <= 0) {
        g_anim_y = 0;
        g_anim_dy = -g_anim_dy;
    } else if ((u64)g_anim_y >= g_anim_max_y) {
        g_anim_y = (i64)g_anim_max_y;
        g_anim_dy = -g_anim_dy;
    }

    fb_clear(0x00101014);
    fb_draw_image((u64)g_anim_x, (u64)g_anim_y, &innovatios_logo_image, g_logo_size);
    fb_present();
}
#endif

static void poll_keyboard_demo(void) {
#if CONFIG_KEYBOARD && CONFIG_SERIAL
    while (keyboard_has_char()) {
        char c = keyboard_read_char();
        serial_write("kernel: key: ");
        serial_putc(c);
        serial_write("\n");
    }
#endif
}

void scheduler_init(u64 hhdm_offset) {
    g_hhdm_offset = hhdm_offset;
    g_task_count = 0;
    g_current_task = 0;
    g_next_pid = 1;
    g_switch_count = 0;
    g_scheduler_running = false;
    g_anim_x = 100;
    g_anim_y = 100;
    g_anim_dx = 4;
    g_anim_dy = 3;
    g_last_tick_log = 0;

    k_memset(g_tasks, 0, sizeof(g_tasks));

#if CONFIG_FRAMEBUFFER
    g_anim_max_x = (fb_width()  > innovatios_logo_image.width * g_logo_size)
        ? (fb_width()  - (innovatios_logo_image.width * g_logo_size))
        : 0;
    g_anim_max_y = (fb_height() > innovatios_logo_image.height * g_logo_size)
        ? (fb_height() - (innovatios_logo_image.height * g_logo_size))
        : 0;
#else
    g_anim_max_x = 0;
    g_anim_max_y = 0;
#endif

    setup_task(&g_tasks[g_task_count++], "ring3-alpha", 0);
    setup_task(&g_tasks[g_task_count++], "ring3-beta", 1);

#if CONFIG_SERIAL
    serial_write("scheduler: demo tasks prepared\n");
#endif
}

void scheduler_on_timer(interrupt_frame_t *frame) {

    if (g_task_count == 0) {
        return;
    }

#if CONFIG_FRAMEBUFFER
    update_demo_overlay();
#endif
    poll_keyboard_demo();

#if CONFIG_PIT
    u64 ticks = pit_get_ticks();
    if (ticks - g_last_tick_log >= 100) {
#if CONFIG_SERIAL
        serial_write("kernel: tick\n");
#endif
        g_last_tick_log = ticks;
    }
#endif

    if (!g_scheduler_running || g_task_count < 2) {
        return;
    }

    process_t *current = &g_tasks[g_current_task];
    k_memcpy(&current->frame, frame, sizeof(*frame));

    g_current_task = (g_current_task + 1) % g_task_count;
    process_t *next = &g_tasks[g_current_task];
    k_memcpy(frame, &next->frame, sizeof(*frame));

    g_switch_count++;
    if (g_switch_count <= 8u || (g_switch_count % 50u) == 0u) {
        serial_write("scheduler: switch #");

        char count_buf[21];
        int count_i = 20;
        count_buf[count_i--] = '\0';
        u64 count_value = g_switch_count;
        if (count_value == 0) {
            count_buf[count_i--] = '0';
        } else {
            while (count_value > 0) {
                count_buf[count_i--] = (char)('0' + (count_value % 10));
                count_value /= 10;
            }
        }
        serial_write(&count_buf[count_i + 1]);

        serial_write(": ");
        serial_write(current->name);
        serial_write(" -> ");
        serial_write(next->name);
        serial_write(" (pid ");

        char buf[21];
        int pid_i = 20;
        buf[pid_i--] = '\0';
        u64 value = next->pid;
        if (value == 0) {
            buf[pid_i--] = '0';
        } else {
            while (value > 0) {
                buf[pid_i--] = (char)('0' + (value % 10));
                value /= 10;
            }
        }
        serial_write(&buf[pid_i + 1]);
        serial_write(")\n");
    }
}

static __attribute__((noreturn)) void enter_frame(interrupt_frame_t *frame) {
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "pop %%r15\n"
        "pop %%r14\n"
        "pop %%r13\n"
        "pop %%r12\n"
        "pop %%r11\n"
        "pop %%r10\n"
        "pop %%r9\n"
        "pop %%r8\n"
        "pop %%rdi\n"
        "pop %%rsi\n"
        "pop %%rbp\n"
        "pop %%rdx\n"
        "pop %%rcx\n"
        "pop %%rbx\n"
        "pop %%rax\n"
        "add $16, %%rsp\n"
        "iretq\n"
        :
        : "r"(frame)
        : "memory"
    );
    __builtin_unreachable();
}

void scheduler_start_demo(void) {
    if (g_task_count == 0) {
        scheduler_fatal("scheduler: no demo tasks available");
    }

    g_scheduler_running = true;
    serial_write("scheduler: entering ring3 demo\n");
    enter_frame(&g_tasks[0].frame);
}
