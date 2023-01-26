package com.samsung.lwe.escargot;

public class Memory {
    static { Escargot.init(); }

    static public native void gc();

    static public native long heapSize(); // Return the number of bytes in the heap.  Excludes bdwgc private data structures. Excludes the unmapped memory
    static public native long totalSize(); // Return the total number of bytes allocated in this process

    // NOTE bdwgc(c/c++ gc library escargot use) allocate at least N/GC_free_space_divisor bytes between collections
    // (Allocated memory by GC x 2) / (Frequency parameter value)
    // Increasing this value may use less space but there is more collection event
    static public native void setGCFrequency(int value);
}
