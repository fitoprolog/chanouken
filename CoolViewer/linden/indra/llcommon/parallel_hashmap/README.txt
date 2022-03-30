This directory contains files from the parallel-hashmap project by Gregory
Popovitch (https://github.com/greg7mdp/parallel-hashmap).

They are better kept with the viewer sources instead of being donwloaded
separately as a "pre-built library package", especially since they constitute
a header-only library common to all OSes.

Plus, I slightly modified the files to:
 - better integrate with the viewer code (with the use of LL_[NO_]INLINE),
 - provide a minor speed optimization (PHMAP_NO_MIXING),
 - allow the use of sse2neon.h with ARM64 builds, in order to use the optimized
   SSE2/SSSE3 code (which then gets automatically translated into their NEON
   counterparts).

HB
