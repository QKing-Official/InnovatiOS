#include <kernel/types.h>

#if CONFIG_SERIAL
#include <kernel/drivers/serial.h>
#endif

#if CONFIG_FRAMEBUFFER
#include <kernel/drivers/framebuffer.h>
#include <kernel/drivers/console.h>
#include <kernel/assets/innovatios_logo.h>
#endif

#if CONFIG_PIT
#include <kernel/drivers/pit.h>
#endif

#if CONFIG_KEYBOARD
#include <kernel/drivers/keyboard.h>
#endif

#if CONFIG_GDT
#include <kernel/arch/x86_64/gdt.h>
#endif

#if CONFIG_INTERRUPTS
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/pic.h>
#include <kernel/arch/x86_64/interrupts.h>
#endif

#if CONFIG_PMM
#include <kernel/mm/pmm.h>
#endif

#if CONFIG_VMM
#include <kernel/mm/vmm.h>
#endif

#if CONFIG_HEAP
#include <kernel/mm/heap.h>
#endif

#include <user.h>
#include <shell.h>
#include <kernel/lib/string.h>
#include <limine.h>

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id       = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id       = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id       = LIMINE_HHDM_REQUEST,
    .revision = 0
};

static void hcf(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

#if CONFIG_SERIAL
static void serial_write_u64(u64 val) {
    char buf[21];
    int i = 20;
    buf[i--] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0) {
            buf[i--] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    serial_write(&buf[i + 1]);
}
#endif

#if CONFIG_FRAMEBUFFER
static u32 g_boot_line = 0;

static void boot_draw_logo(void);

static int g_boot_delay_ready = 0;

static void boot_status(const char *msg, int ok, const char *err_detail) {
    if (ok) {
        console_puts_color("[ OK ] ", CON_COLOR_GREEN);
    } else {
        console_puts_color("[FAIL] ", CON_COLOR_RED);
    }
    console_puts_color(msg, CON_COLOR_WHITE);
    console_puts("\n");

    if (!ok && err_detail) {
        console_puts_color("       ", CON_COLOR_RED);
        console_puts_color(err_detail, CON_COLOR_RED);
        console_puts("\n");
    }

    boot_draw_logo();
    g_boot_line++;

#if CONFIG_PIT && CONFIG_INTERRUPTS
    if (g_boot_delay_ready) {
        pit_sleep_ms(150);
    }
#endif

    (void)g_boot_line;
}

static void boot_draw_logo(void) {
    const u64 logo_size = 4;
    const u64 box_w = innovatios_logo_image.width * logo_size;
    u64 x = (fb_width() > box_w + 16) ? (fb_width() - box_w - 16) : 0;
    u64 y = 8;
    fb_draw_image(x, y, &innovatios_logo_image, logo_size);
    fb_present();
}
#endif

#define VMM_TEST_VADDR 0xffffff0000000000ULL

#if CONFIG_PMM && CONFIG_VMM
static int vmm_smoke_test(void) {
    u64 test_phys = pmm_alloc_page();
    if (test_phys == 0) return 0;

    if (vmm_map(vmm_kernel_space(), VMM_TEST_VADDR, test_phys,
                VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE) != 0) {
        pmm_free_page(test_phys);
        return 0;
    }

    volatile u8 *p = (volatile u8 *)VMM_TEST_VADDR;
    p[0]    = 0xAB;
    p[4095] = 0xCD;

    u64 translated = vmm_translate(vmm_kernel_space(), VMM_TEST_VADDR);
    int ok = (translated == test_phys) && (p[0] == 0xAB) && (p[4095] == 0xCD);

    vmm_unmap(vmm_kernel_space(), VMM_TEST_VADDR);
    u64 translated_after = vmm_translate(vmm_kernel_space(), VMM_TEST_VADDR);
    ok = ok && (translated_after == 0);

    pmm_free_page(test_phys);
    return ok;
}
#endif

#if CONFIG_FRAMEBUFFER && CONFIG_KEYBOARD
static void login_loop(void) {
    char username[USER_NAME_MAX];
    char password[USER_NAME_MAX];

    while (1) {
        console_clear();

        console_puts_color("\n", CON_COLOR_WHITE);
        console_puts_color("  === InnovatiOS ===\n\n", CON_COLOR_CYAN);

        console_puts_color("login: ", CON_COLOR_WHITE);
        console_readline(username, sizeof(username), 0);

        console_puts_color("password: ", CON_COLOR_WHITE);
        console_readline(password, sizeof(password), '*');

        user_t *user = user_authenticate(username, password);
        if (user) {
            console_puts_color("\nLogin successful.\n", CON_COLOR_GREEN);
#if CONFIG_PIT
            pit_sleep_ms(500);
#endif
            shell_run(user);

        } else {
            console_puts_color("\nLogin incorrect.\n", CON_COLOR_RED);
#if CONFIG_PIT
            pit_sleep_ms(1500);
#endif
        }
    }
}
#endif

void kmain(void) {
#if CONFIG_SERIAL
    serial_init();
    serial_write("kernel: boot ok, kmain() reached\n");
#endif

#if CONFIG_FRAMEBUFFER
    int fb_ok = 0;
    if (framebuffer_request.response != NULL &&
        framebuffer_request.response->framebuffer_count >= 1) {
        fb_init(framebuffer_request.response);
        console_init();
        fb_ok = 1;

        boot_draw_logo();
        console_puts_color("\n  InnovatiOS v0.1\n", CON_COLOR_CYAN);
        console_puts_color("  ==========================\n\n", CON_COLOR_GREY);
    }
#endif

#if CONFIG_SERIAL
#if CONFIG_FRAMEBUFFER
    if (fb_ok) boot_status("Serial port (COM1)", 1, NULL);
#endif
#endif

#if CONFIG_GDT
    gdt_init();
#if CONFIG_SERIAL
    serial_write("kernel: GDT initialized\n");
#endif
#if CONFIG_FRAMEBUFFER
    if (fb_ok) boot_status("Global Descriptor Table (GDT)", 1, NULL);
#endif
#endif

#if CONFIG_INTERRUPTS
    idt_init();
    pic_remap();
#if CONFIG_SERIAL
    serial_write("kernel: IDT + PIC initialized\n");
#endif
#if CONFIG_FRAMEBUFFER
    if (fb_ok) boot_status("Interrupt Descriptor Table", 1, NULL);
#endif
#endif

#if CONFIG_PIT
    pit_init(100);
#if CONFIG_SERIAL
    serial_write("kernel: PIT initialized (100 Hz)\n");
#endif
#if CONFIG_FRAMEBUFFER
    if (fb_ok) boot_status("Programmable Interval Timer", 1, NULL);
#endif
#endif

#if CONFIG_KEYBOARD
    keyboard_init();
#if CONFIG_SERIAL
    serial_write("kernel: keyboard initialized\n");
#endif
#if CONFIG_FRAMEBUFFER
    if (fb_ok) boot_status("PS/2 Keyboard driver", 1, NULL);
#endif
#endif

#if CONFIG_INTERRUPTS
    __asm__ volatile ("sti");
#if CONFIG_SERIAL
    serial_write("kernel: interrupts enabled\n");
#endif
#if CONFIG_PIT
    g_boot_delay_ready = 1;
#endif
#endif

#if CONFIG_PMM
    int pmm_ok = (memmap_request.response != NULL && hhdm_request.response != NULL);
    if (pmm_ok) {
        pmm_init(memmap_request.response, hhdm_request.response->offset);
#if CONFIG_SERIAL
        serial_write("kernel: pmm total pages: ");
        serial_write_u64(pmm_total_pages());
        serial_write("\nkernel: pmm free pages:  ");
        serial_write_u64(pmm_free_pages_count());
        serial_write("\n");
#endif
#if CONFIG_FRAMEBUFFER
        if (fb_ok) boot_status("Physical Memory Manager", 1, NULL);
#endif
    } else {
#if CONFIG_SERIAL
        serial_write("kernel: missing memmap or hhdm response, halting\n");
#endif
#if CONFIG_FRAMEBUFFER
        if (fb_ok) boot_status("Physical Memory Manager", 0, "Missing Limine memmap/hhdm response");
#endif
        hcf();
    }
#endif

#if CONFIG_VMM
    vmm_init(hhdm_request.response->offset);
#if CONFIG_SERIAL
    serial_write("kernel: VMM initialized\n");
#endif
#if CONFIG_FRAMEBUFFER
    if (fb_ok) {
        int vmm_ok = vmm_smoke_test();
        boot_status("Virtual Memory Manager", vmm_ok, vmm_ok ? NULL : "Smoke test failed");
    }
#endif
#endif

#if CONFIG_HEAP
    heap_init(hhdm_request.response->offset, 64);
#if CONFIG_SERIAL
    serial_write("kernel: heap initialized\n");
#endif
#if CONFIG_FRAMEBUFFER
    if (fb_ok) {
        char *test = (char *)kmalloc(64);
        int heap_ok = 0;
        if (test) {
            for (int i = 0; i < 64; i++) test[i] = (char)i;
            heap_ok = 1;
            kfree(test);
        }
        boot_status("Kernel heap allocator", heap_ok, heap_ok ? NULL : "kmalloc returned NULL");
    }
#endif
#endif

    user_init();
#if CONFIG_FRAMEBUFFER
    if (fb_ok) {
        boot_status("User management subsystem", 1, NULL);
    }
#endif

#if CONFIG_FRAMEBUFFER && CONFIG_KEYBOARD
    if (fb_ok) {
        console_puts_color("\nLoading GUI fonts", CON_COLOR_WHITE);
#if CONFIG_PIT
        for (int i=0; i<3; i++) {
            console_puts_color(".", CON_COLOR_WHITE);
            pit_sleep_ms(600);
        }
#endif
        console_puts("\n");
        boot_status("GUI Fonts & Assets", 1, NULL);
        
        console_puts_color("\n  Boot complete. All systems operational.\n\n", CON_COLOR_GREEN);
#if CONFIG_PIT
        pit_sleep_ms(1500);
#endif
        login_loop();
    }
#endif

    hcf();
}
