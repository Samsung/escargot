package com.samsung.lwe.escargot.shell;

import androidx.appcompat.app.AppCompatActivity;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import com.samsung.lwe.escargot.Bridge;
import com.samsung.lwe.escargot.Context;
import com.samsung.lwe.escargot.Evaluator;
import com.samsung.lwe.escargot.Globals;
import com.samsung.lwe.escargot.Memory;
import com.samsung.lwe.escargot.VMInstance;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.util.Optional;

public class MainActivity extends AppCompatActivity {

    private void copyAssets() {
        AssetManager assetManager = getAssets();
        String[] files = null;
        try {
            files = assetManager.list("");
        } catch (IOException e) {
            Log.e("tag", "Failed to get asset file list.", e);
        }
        for(String filename : files) {
            InputStream in = null;
            OutputStream out = null;
            try {
                in = assetManager.open(filename);
                File outFile = new File(getApplicationContext().getFilesDir(), filename);
                out = new FileOutputStream(outFile);
                copyFile(in, out);
                in.close();
                in = null;
                out.flush();
                out.close();
                out = null;
            } catch(IOException e) {
                Log.e("tag", "Failed to copy asset file: " + filename, e);
            }
        }

        Log.e("Escargot shell", getApplicationContext().getFilesDir().getAbsolutePath());
    }

    private void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while((read = in.read(buffer)) != -1){
            out.write(buffer, 0, read);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // copy assets to internal storage (for copying js files which are used by test)
        copyAssets();

        // TODO make TC & add CI
        Globals.initializeGlobals();

        Log.i("Escargot", Globals.buildDate());
        Log.i("Escargot", Globals.version());

        Memory.setGCFrequency(24);

        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.empty());
        Context context = Context.create(vmInstance);

        Evaluator.evalScript(context, "a = 1", "from_java.js", true);
        Evaluator.evalScript(context, "a", "from_java2.js", true);
        Evaluator.evalScript(context, "a = new Date()", "from_java3.js", true);
        Evaluator.evalScript(context, "const koDtf = new Intl.DateTimeFormat(\"ko\", { dateStyle: \"long\" }); print(koDtf.format(a))", "from_java4.js", true);

        Bridge.register(context, "Native", "addString", new Bridge.Adapter() {

            @Override
            public Optional<String> callback(Optional<String> data) {
                return Optional.of(data.get() + "ASdfasdfasdf");
            }
        });

        Bridge.register(context, "Native", "returnString", new Bridge.Adapter() {

            @Override
            public Optional<String> callback(Optional<String> data) {
                Log.i("Escargot", "empty parameter test->");
                if (data.isPresent()) {
                    Log.i("Escargot", "bad");
                } else {
                    Log.i("Escargot", "good");
                }
                return Optional.of("string from java");
            }
        });

        Bridge.register(context, "Native", "returnNothing", new Bridge.Adapter() {

            @Override
            public Optional<String> callback(Optional<String> data) {
                return Optional.empty();
            }
        });

        Evaluator.evalScript(context, "Native.addString('dddd')", "from_java5.js", true);
        Evaluator.evalScript(context, "Native.returnString()", "from_java6.js", true);
        Evaluator.evalScript(context, "print('ret? expect undefined'); print(Native.returnNothing())", "from_java7.js", true);

        context.destroy();
        context.hasValidNativePointer(); // -> false
        vmInstance.destroy();
        vmInstance.hasValidNativePointer(); // -> false

        Globals.finalizeGlobals();
    }
}