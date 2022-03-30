This directory contains files that are distributed as "pre-built libraries" by
Linden Lab, but that in fact only contained headers pertaining to glext
(c)2007-2010 The Khronos Group Inc.
They are better kept with the viewer sources, especially since they never
changed and are either common to all OSes or not used by some (i.e. they don't
incur any OS-specific conflict by their mere presence here).
Oh, and there are also a few changes applied to these headers (among which a
crash fix)... 

HB
