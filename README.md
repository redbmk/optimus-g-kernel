NOTE
====
    This is not currently working (i.e. DOES NOT COMPILE) and therefore should only be used
    for reference or to help with debugging. I will be making commits as I go and will update
    this when I have a kernel that compiles.

DESCRIPTION
===========
    This will be the kernel for the Optimus G.

    I am specifically working on the 3.4.0 kernel for the Sprint LG Optimus G (LS970),
however I plan to include a branch for the 3.0.21 kernel and am hoping to have it working
for all variations of the OG (although I only have the Sprint version with which to test).

STATUS
======
    Compilation is failing with the following error:

    arch/arm/kernel/armksyms.c:159: error: '__pv_phys_offset' undeclared here (not in a function)
    arch/arm/kernel/armksyms.c:159: warning: type defaults to 'int' in declaration of '__pv_phys_offset'
    error, forbidden warning: armksyms.c:159
