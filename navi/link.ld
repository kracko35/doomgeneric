ENTRY(_start)
SECTIONS {
  . = 0x8c000000;
  .text : { KEEP(*(.text.entry)) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  . = ALIGN(4);
  __bss_start = .;
  .bss (NOLOAD) : { *(.bss*) *(COMMON) }
  . = ALIGN(4);
  __bss_end = .;
  ASSERT(. < 0x8d000000, "code and globals overlap heap")
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
