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
            //
            // If the jar was built with WASM support on, libescargot-jni is
            // dynamically linked against libwalrus (see bundleHostJar in
            // build.gradle), but the extracted-to-a-random-tempfile .so
            // below has no RPATH pointing back at it. Load libwalrus first
            // if the jar carries one: dlopen/dyld resolve a
            // DT_NEEDED/LC_LOAD_DYLIB entry against any already-loaded
            // library with a matching SONAME/install name before searching
            // any paths, so this satisfies libescargot-jni's dependency
            // without needing the two temp files to be colocated. When the
            // jar has no libwalrus.so resource (WASM off, the common case),
            // this is a silent no-op.
            InputStream walrusStream = Escargot.class.getResourceAsStream("/libwalrus.so");
            if (walrusStream != null) {
                copyStreamAsTempFile(walrusStream, "libwalrus-", ".so", true).ifPresent(System::load);
            }

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
