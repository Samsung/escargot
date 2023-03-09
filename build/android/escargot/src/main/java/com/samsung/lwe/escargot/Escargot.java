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
            tryToLoadLibraryFromJarResource(e);
        }
    }
    static native public void init();
    static private synchronized void tryToLoadLibraryFromJarResource(UnsatisfiedLinkError e)
    {
        InputStream soStream = Escargot.class.getResourceAsStream("/libescargot-jni.so");
        if (soStream != null) {
            try {
                File f = File.createTempFile("libescargot-jni-", ".so");
                f.deleteOnExit();
                f.setExecutable(true);
                FileOutputStream fos = new FileOutputStream(f);
                int read;
                byte[] bytes = new byte[1024];
                while ((read = soStream.read(bytes)) != -1) {
                    fos.write(bytes, 0, read);
                }
                fos.flush();
                soStream.close();
                fos.close();
                System.load(f.getAbsolutePath());
            } catch (IOException e1) {
                e1.printStackTrace();
            }
        } else {
            throw e;
        }
    }
}
