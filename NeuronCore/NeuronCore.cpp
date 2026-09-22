// A shared-items translation unit: this file is compiled into every project that imports
// NeuronCore.vcxitems, each of which has its own precompiled header. There is no one pch to share
// between them, so these files use none and include their master header first instead.

#include "NeuronCore.h"

// THIS TRANSLATION UNIT IS DELIBERATELY EMPTY. Every project needs one source file so that
// MSBuild has something to compile and a precompiled header has somewhere to be created; this
// library's code is entirely in headers and in its other sources. It held the library-name
// placeholder until M0 closed, which is the commit that removed it.
