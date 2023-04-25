package com.samsung.lwe.escargot;

final public class Memory {
    static { Escargot.init(); }
    private Memory() {
    }

    static public native void gc();

    /**
     * Return the number of bytes in the heap.  Excludes bdwgc private data structures. Excludes the unmapped memory
     * @return
     */
    static public native long heapSize();

    /**
     * Return the total number of bytes allocated in this process
     * @return
     */
    static public native long totalSize();

    /**
     * NOTE bdwgc(c/c++ gc library escargot use) allocate at least N/GC_free_space_divisor bytes between collections
     * (Allocated memory by GC x 2) / (Frequency parameter value)
     * Increasing this value may use less space but there is more collection event
     * @param value
     */
    static public native void setGCFrequency(int value);
}
