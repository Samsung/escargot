package com.samsung.lwe.escargot;

import java.util.Optional;

public class Bridge {
    public abstract static class Adapter {
        public abstract Optional<String> callback(Optional<String> data);
    }
    
    static public native boolean register(Context context, String objectName, String propertyName, Adapter adapter); 
}
