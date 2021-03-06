#include "xmemory.h"

/// The globals region.
xglobals xmemory::_globals;

/// The protected heap used to satisfy small objects requirement. Less than 256 bytes now.
warpheap<xdefines::NUM_HEAPS, xdefines::PROTECTEDHEAP_CHUNK, xoneheap<xheap<PROTECTEDHEAP_SIZE> > > xmemory::_pheap;


/// A signal stack, for catching signals.
stack_t xmemory::_sigstk;

int xmemory::_heapid;
