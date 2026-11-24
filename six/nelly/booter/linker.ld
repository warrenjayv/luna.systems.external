ENTRY(_start)

SECTIONS
{
   //\ section at 1M
   . = 1M;
   //\ code section
   .text : 
   {
     KEEP(*(.multiboot))
     *(.text)     
   }
   //\ read only
   .rodata : 
   {
        *(.rodata*)
   }
   //\ global variables
   .data : 
   {
        
   }
   //\ stack
   .bss : 
   {
     *(COMMON)
     *(.bss)
   }
}
