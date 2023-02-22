package com.samsung.lwe.escargot;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Paths;

public class Escargot {
    static {
        try {
            System.loadLibrary("escargot-jni");
        } catch(UnsatisfiedLinkError e) {
            // try to load from jar
            InputStream soStream = Escargot.class.getResourceAsStream("/libescargot-jni.so");
            if (soStream != null) {
                try {
                    if (Files.size(Paths.get("/tmp/libescargot-jni.so")) == soStream.available()) {
                        System.load("/tmp/libescargot-jni.so");
                    } else {
                        throw new Exception();
                    }
                } catch(Exception e2) {
                    try {
                        File f = new File("/tmp/libescargot-jni.so");
                        f.setExecutable(true);
                        FileOutputStream fos = new FileOutputStream(f);
                        int read;
                        byte [] bytes = new byte[1024];
                        while ((read = soStream.read(bytes)) != -1) {
                            fos.write(bytes, 0, read);
                        }
                        fos.flush();
                    } catch (IOException e1) {
                        e1.printStackTrace();
                    }
                    System.load("/tmp/libescargot-jni.so");
                }
            } else {
                throw e;
            }
        }
    }

    static native public void init();
}
