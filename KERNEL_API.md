# Kernel API Reference

This documents every function currently available inside the kernel: what
it does, its signature, which header to include, and any gotchas.

**Important caveat:** most of this is still kernel-internal API. There is
now a small ring-3 demo scheduler and an `iretq` transition path, but
there is still no syscall interface and no ELF loader, so the user-mode
tasks are demo processes built by the kernel itself. A userspace C
program still cannot `#include` these headers and call them directly,
since it runs in a different privilege level with no direct access to
kernel data structures like `g_back_buffer`.

Treat this file as the "what exists so far" map, and update it every time
you add a new module.

---

## `kernel/types.h`

Basic fixed-width type aliases used everywhere in the kernel.

```c
typedef uint64_t u64;  typedef int64_t i64;
typedef uint32_t u32;  typedef int32_t i32;
typedef uint16_t u16;  typedef int16_t i16;
typedef uint8_t  u8;   typedef int8_t  i8;
```

Also pulls in `stdint.h`, `stddef.h`, `stdbool.h` (freestanding-safe, no
libc dependency issues — these are compiler-provided headers).

---

## `kernel/lib/string.h` — freestanding memory/string helpers

No libc is linked into the kernel, so these are hand-rolled replacements
for the handful of `string.h` functions the kernel needs. Prefixed `k_`
to avoid any future collision with a real libc once one exists
(e.g. for userspace).

| Function | Signature | Description |
|---|---|---|
| `k_memcpy` | `void *k_memcpy(void *dst, const void *src, size_t n)` | Byte-by-byte copy, non-overlapping regions only. |
| `k_memset` | `void *k_memset(void *dst, int val, size_t n)` | Fills `n` bytes with `val` (low byte only, like libc `memset`). |
| `k_memmove` | `void *k_memmove(void *dst, const void *src, size_t n)` | Like `k_memcpy` but safe for overlapping regions. |
| `k_strlen` | `size_t k_strlen(const char *s)` | Length of a NUL-terminated string. |

---

## `kernel/drivers/serial.h` — COM1 serial output

Your debugging lifeline before you have any other console. Writes to
COM1 (`0x3F8`) via port I/O; converts `\n` to `\r\n` automatically.

| Function | Signature | Description |
|---|---|---|
| `serial_init` | `void serial_init(void)` | Configures COM1: 38400 baud, 8N1, FIFO enabled. Call once, early in `kmain`. |
| `serial_putc` | `void serial_putc(char c)` | Writes a single character, blocking until the UART's TX buffer is free. |
| `serial_write` | `void serial_write(const char *str)` | Writes a NUL-terminated string. |
| `serial_write_len` | `void serial_write_len(const char *str, size_t len)` | Writes exactly `len` bytes (use when the string isn't NUL-terminated, or you want to avoid an extra `strlen`-style scan). |

---

## `kernel/drivers/framebuffer.h` — double-buffered graphics

Draws into an off-screen back buffer; nothing is visible until you call
`fb_present()`. This is what prevents flicker/tearing when animating —
draw a whole frame, then flip it to screen in one copy.

| Function | Signature | Description |
|---|---|---|
| `fb_init` | `void fb_init(void *limine_fb_response)` | One-time setup from Limine's framebuffer response. Call once in `kmain` after checking `framebuffer_request.response` is non-NULL. |
| `fb_width` | `u64 fb_width(void)` | Current mode's width in pixels (clamped to `FB_MAX_WIDTH`). |
| `fb_height` | `u64 fb_height(void)` | Current mode's height in pixels (clamped to `FB_MAX_HEIGHT`). |
| `fb_put_pixel` | `void fb_put_pixel(u64 x, u64 y, u32 rgb)` | Sets one pixel in the **back buffer**. Out-of-bounds `x`/`y` is silently ignored. `rgb` is `0x00RRGGBB`. |
| `fb_clear` | `void fb_clear(u32 rgb)` | Fills the entire back buffer with one color. |
| `fb_fill_rect` | `void fb_fill_rect(u64 x, u64 y, u64 w, u64 h, u32 rgb)` | Fills a rectangle in the back buffer. Clips to screen bounds. |
| `fb_draw_image` | `void fb_draw_image(u64 x, u64 y, const fb_image_t *image, u64 size)` | Alpha-blits a 32-bit sprite into the back buffer, scaled up by an integer `size` factor using nearest-neighbor sampling. Pixels are row-major `0xAARRGGBB`; `0x00` is transparent, `0xFF` is opaque, and values in between are blended over whatever is already in the back buffer. |
| `fb_present` | `void fb_present(void)` | Copies the back buffer to the real, visible framebuffer. Call once per frame, after all drawing for that frame is done. |

Constants: `FB_MAX_WIDTH` (1920), `FB_MAX_HEIGHT` (1080) — the back
buffer is a static array sized to these; bump them if you need a bigger
mode, shrink them if you're memory-constrained.

`fb_image_t` is the lightweight sprite wrapper used by `fb_draw_image`:

```c
typedef struct {
  const u32 *pixels; /* 0xAARRGGBB, row-major */
  u64 width;
  u64 height;
} fb_image_t;
```

Typical usage for a 16x16 logo:

```c
static const u32 logo_pixels[16 * 16] = {
  0x00000000, 0x00000000, /* ... */
};

static const fb_image_t logo = {
  .pixels = logo_pixels,
  .width = 16,
  .height = 16,
};

fb_draw_image(x, y, &logo, 4);
```

Use `0xAARRGGBB` values if you want transparency. If you only want an
opaque sprite, set alpha to `0xFF` for every pixel.

**Not yet implemented:** text/font rendering, multiple
framebuffers/windows, anything GPU-accelerated. This is still software
pixel plotting, but sprites can now be alpha-blended through
`fb_draw_image`.

---

## `kernel/arch/x86_64/io.h` — raw port I/O

Thin wrappers around the `in`/`out` x86 instructions. Used by any driver
that talks to hardware over I/O ports (PIC, PIT, and eventually PS/2
keyboard, ATA, etc).

| Function | Signature | Description |
|---|---|---|
| `outb` | `void outb(u16 port, u8 val)` | Writes one byte to an I/O port. |
| `inb` | `u8 inb(u16 port)` | Reads one byte from an I/O port. |
| `io_wait` | `void io_wait(void)` | Short delay (dummy write to port `0x80`) for hardware that needs breathing room between successive I/O operations during setup sequences. |

---

## `kernel/arch/x86_64/pic.h` — 8259 PIC remap + EOI

Remaps the legacy PIC so hardware IRQs don't collide with CPU exception
vectors, and provides the End-Of-Interrupt signal every IRQ handler must
send.

| Function | Signature | Description |
|---|---|---|
| `pic_remap` | `void pic_remap(void)` | Remaps IRQ0-7 → vectors 32-39, IRQ8-15 → vectors 40-47. Masks everything except IRQ0 (timer) since no other IRQ handlers exist yet. Call once, before `sti`. |
| `pic_send_eoi` | `void pic_send_eoi(u8 irq)` | Must be called at the end of every hardware IRQ handler (with the *original* IRQ number, 0-15), or that IRQ line will never fire again. |

---

## `kernel/arch/x86_64/idt.h` — interrupt descriptor table

Sets up all 256 IDT entries. CPU exceptions and hardware IRQs now flow
through a shared interrupt trampoline and dispatcher so the kernel can
log real fault state instead of printing one generic halt message.

| Function | Signature | Description |
|---|---|---|
| `idt_init` | `void idt_init(void)` | Builds and loads the IDT (`lidt`). Call once, before `pic_remap()` and before `sti`. |

The actual dispatch logic now lives in `kernel/arch/x86_64/interrupts.h`
and `kernel/arch/x86_64/interrupts.c`.

| Function | Signature | Description |
|---|---|---|
| `arch_interrupt_dispatch` | `void arch_interrupt_dispatch(interrupt_frame_t *frame)` | Common C-side interrupt/exception dispatcher. Timer IRQs advance the scheduler, keyboard IRQs drain the keyboard driver, and CPU exceptions are printed with register state and error code. |
| `exception_demo_showcase` | `void exception_demo_showcase(void)` | Prints a synthetic page-fault report from `kmain()` to show the exception formatting without crashing the kernel. |

`interrupt_frame_t` is the kernel's shared saved-state layout for the
assembly trampolines and scheduler handoff. It contains general-purpose
registers plus the trap vector, error code, RIP, CS, RFLAGS, RSP, and SS.

## `kernel/arch/x86_64/gdt.h` — GDT and TSS selectors

The kernel now uses a real TSS-backed ring transition path. The GDT still
provides kernel code/data segments, but it also defines user code/data
selectors used by the ring-3 demo scheduler.

| Constant | Value | Description |
|---|---|---|
| `GDT_KERNEL_CODE` | `0x08` | Kernel code segment selector. |
| `GDT_KERNEL_DATA` | `0x10` | Kernel data segment selector. |
| `GDT_USER_CODE` | `0x18` | User code segment base selector; OR with `3` for a ring-3 selector (`0x1B`). |
| `GDT_USER_DATA` | `0x20` | User data segment base selector; OR with `3` for a ring-3 selector (`0x23`). |
| `GDT_TSS` | `0x28` | Task State Segment selector loaded with `ltr`. |

`gdt_init()` sets `rsp0` in the TSS so a ring-3 exception can safely
return to ring-0 using the kernel stack.

## `kernel/proc/scheduler.h` — demo process scheduler

This is a small round-robin scheduler for the current demo. It is not a
full process subsystem yet, but it does create PCB-like state, save and
restore register context on timer ticks, and use `iretq` to enter user
mode.

| Function | Signature | Description |
|---|---|---|
| `scheduler_init` | `void scheduler_init(u64 hhdm_offset)` | Allocates and maps the demo ring-3 tasks, sets up their initial interrupt frames, and prepares the scheduler state. |
| `scheduler_on_timer` | `void scheduler_on_timer(interrupt_frame_t *frame)` | Called from the timer interrupt path. Saves the current task state into its PCB and loads the next task into the live interrupt frame. |
| `scheduler_start_demo` | `void scheduler_start_demo(void)` | Enters the first ring-3 task with `iretq`. This never returns. |

The demo tasks are tiny in-memory loops, which keeps the ring transition
and context-switch machinery isolated from ELF loading and syscalls.

## `kernel/arch/x86_64/interrupts.h` — shared interrupt frame and dispatcher

| Type / Function | Signature | Description |
|---|---|---|
| `interrupt_frame_t` | struct | Shared saved-state layout used by the assembly trampolines, exception printer, and scheduler. |
| `arch_interrupt_dispatch` | `void arch_interrupt_dispatch(interrupt_frame_t *frame)` | Common entry point used by all CPU exceptions and hardware IRQs. |
| `exception_demo_showcase` | `void exception_demo_showcase(void)` | Serial-only diagnostic demo that prints a representative page-fault frame. |

---

## `kernel/drivers/pit.h` — 8253/8254 timer (IRQ0)

Programmable Interval Timer. This is what gives you real elapsed time
and the ability to sleep without burning CPU cycles in a busy loop.

| Function | Signature | Description |
|---|---|---|
| `pit_init` | `void pit_init(u32 hz)` | Programs the PIT to fire IRQ0 at `hz` times/sec (e.g. `pit_init(100)` → a tick every ~10ms). Call after `idt_init()`/`pic_remap()`, before `sti`. |
| `pit_handle_irq` | `void pit_handle_irq(void)` | Kernel-side IRQ0 helper called by the shared interrupt dispatcher. It increments the tick counter; the dispatcher sends the PIC EOI after the handler returns. |
| `pit_get_ticks` | `u64 pit_get_ticks(void)` | Ticks elapsed since `pit_init()` was called. Each tick = `1/hz` seconds. |
| `pit_sleep_ms` | `void pit_sleep_ms(u32 ms)` | Blocks (via `hlt`, so the CPU actually sleeps rather than spins) until at least `ms` milliseconds have passed. Requires interrupts enabled and `pit_init()` already called. |

The timer now also drives the demo scheduler, so each tick can trigger a
context switch between ring-3 tasks.

## `kernel/drivers/keyboard.h` — PS/2 keyboard buffer

| Function | Signature | Description |
|---|---|---|
| `keyboard_init` | `void keyboard_init(void)` | Unmasks IRQ1 on the PIC so keyboard interrupts start arriving. |
| `keyboard_handle_irq` | `void keyboard_handle_irq(void)` | Kernel-side IRQ1 helper called by the shared interrupt dispatcher. It reads the scancode and pushes translated characters into the ring buffer. |
| `keyboard_has_char` | `int keyboard_has_char(void)` | Returns nonzero if the ring buffer currently has input queued. |
| `keyboard_read_char` | `char keyboard_read_char(void)` | Blocks until one character is available, then pops it from the buffer. |

---

## Boot-time initialization order

This is the order `kmain()` currently calls things in, and the order any
new subsystem should generally slot into:

```
serial_init()
gdt_init()
idt_init()
pic_remap()
pit_init(hz)
keyboard_init()
sti                     <- interrupts enabled, timer starts ticking
fb_init(...)            <- needs Limine's framebuffer response
scheduler_init(hhdm_offset)
exception_demo_showcase()
scheduler_start_demo()
```

---

## Not implemented yet (planned)

These don't exist yet — listed here so it's clear what "the rest of the
roadmap" maps to, and so this file has an obvious place to grow into as
each one lands:

- **ELF64 loader** — demo tasks are still built by the kernel; there is no
  loader for real user binaries yet.
- **Syscalls** — no userspace/kernel boundary API yet; the current ring-3
  tasks are just scheduler demo loops.
- **Physical/virtual memory manager, kernel heap** (`kernel/mm/`) — no
  `kmalloc`/`kfree` yet; everything so far is static/`.bss` allocation.
- **ACPI parsing** — no shutdown/reboot/multi-core support yet.
- **Filesystem** (`kernel/fs/`) — no way to load a program from disk/initrd yet.
- **libtcc integration** (`kernel/libtcc/`) — depends on the three above.
- **Userspace libc** (`userspace/libc/`) — what programs will actually
  link against once real processes exist; will wrap syscalls, not these
  kernel-internal functions directly.