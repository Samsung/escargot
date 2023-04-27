package com.samsung.lwe.escargot;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Optional;

public class Escargot {
    static {
        try {
            System.loadLibrary("escargot-jni");
        } catch(UnsatisfiedLinkError e) {
            // try to load library from jar resource
            Optional<String> r = copyStreamAsTempFile(Escargot.class.getResourceAsStream("/libescargot-jni.so"),
                    "libescargot-jni-", ".so", true);
            if (r.isPresent()) {
                System.load(r.get());
            } else {
                throw e;
            }
        }
    }
    static public Optional<String> copyStreamAsTempFile(InputStream is, String prefix, String suffix, boolean isExecutable)
    {
        try {
            File f = File.createTempFile(prefix, suffix);
            f.deleteOnExit();
            f.setExecutable(isExecutable);
            FileOutputStream fos = new FileOutputStream(f);
            int read;
            byte[] bytes = new byte[1024];
            while ((read = is.read(bytes)) != -1) {
                fos.write(bytes, 0, read);
            }
            fos.flush();
            is.close();
            fos.close();
            return Optional.of(f.getAbsolutePath());
        } catch (Exception e) {
            e.printStackTrace();
        }
        return Optional.empty();
    }

    static native public void init();
}
