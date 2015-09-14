This is the implementation of the Baggy Bounds runtime for
SAFECode. The details are in 

http://research.microsoft.com/apps/pubs/default.aspx?id=101450

The implementation fails on x86_64 for programs that have byval
arguments. See LLVM bug, http://llvm.org/bugs/show_bug.cgi?id=6965

Also, support for safe CStdLib functions needs to be added.
