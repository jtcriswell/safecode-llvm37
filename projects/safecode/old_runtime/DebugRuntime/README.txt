SafePoolAllocator is basically a safecode runtime which incorporates dangling
pointer detection mechanism through the remapping of a virtual page for every
memory object allocation. This also means that logically, without the recycling
of virtual address, we can never see a same address appearing twice in the
runtime of program compiled and linked with this runtime.


The mechanism involved includes mach specific system calls, such as
mach_vm_remap() used to remap multiple virtual pages to a physical page.
In our current implementation, we also included extra data structure to
keep track of memory allocation and deallocation metadata. This is done by a
struct called DebugMetaData declared in PoolAllocator.h. Upon every memory
object allocation, we allocate extra memory to support this structure.

Upon deallocation, we utilize another system call, i.e. mprotect() to protect
the virtual page which contains the memory object which was intended to be
'freed'. Therefore, upon further reference to such page, the system will
raise a fault, causing the SIG_BUS to be sent to the program.

A signal handler is also written in this runtime in order to catch such signals.
From there onwards, we search through the data structure to obtain the metadata
we wanted. Then, we report this to the user.
NOTE: The signal handler is system specific, i.e., x-86, to obtain information
such as the PC currently faulting.

Our current implementation may also face problems when compiled for 64-bit usage,
only on the event of occurence of dangling error, as underlined by the NOTE above.
