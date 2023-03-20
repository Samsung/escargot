package com.samsung.lwe.escargot.shell;

import androidx.appcompat.app.AppCompatActivity;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import com.samsung.lwe.escargot.Context;
import com.samsung.lwe.escargot.Evaluator;
import com.samsung.lwe.escargot.Globals;
import com.samsung.lwe.escargot.JavaScriptJavaCallbackFunctionObject;
import com.samsung.lwe.escargot.JavaScriptString;
import com.samsung.lwe.escargot.JavaScriptValue;
import com.samsung.lwe.escargot.Memory;
import com.samsung.lwe.escargot.VMInstance;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
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

        final String base = getApplicationContext().getFilesDir().getAbsolutePath() + "/";

        Globals.initializeGlobals();
        Memory.setGCFrequency(24);

        VMInstance vm = VMInstance.create(Optional.empty(), Optional.empty());
        Context context = Context.create(vm);

        {
            Context finalContext = context;
            context.getGlobalObject().set(context, JavaScriptValue.create("print"), JavaScriptJavaCallbackFunctionObject.create(context, "print", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                @Override
                public Optional<JavaScriptValue> callback(JavaScriptValue javaScriptValue, JavaScriptValue[] javaScriptValues) {
                    StringBuffer sb = new StringBuffer();
                    sb.append('\n');
                    for (int i = 0; i < javaScriptValues.length; i ++) {
                        if (i > 0) {
                            System.out.print(" ");
                            sb.append(' ');
                        }

                        Optional<JavaScriptString> s = javaScriptValues[i].toString(finalContext);
                        if (s.isPresent()) {
                            String j = s.get().toJavaString();
                            System.out.print(j);
                            sb.append(j);
                        }
                    }
                    System.out.println();

                    return Optional.empty();
                }
            }));

            context.getGlobalObject().set(context, JavaScriptValue.create("load"), JavaScriptJavaCallbackFunctionObject.create(context, "run", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                @Override
                public Optional<JavaScriptValue> callback(JavaScriptValue javaScriptValue, JavaScriptValue[] javaScriptValues) {
                    Optional<JavaScriptString> s = javaScriptValues[0].toString(finalContext);
                    if (s.isPresent()) {
                        FileReader in = null;
                        try {
                            in = new FileReader(s.get().toJavaString());
                            char[] chars = new char[(int) (new File(s.get().toJavaString())).length()];
                            in.read(chars);
                            String fileContent = new String(chars);
                            return Evaluator.evalScript(finalContext, fileContent, s.get().toJavaString(), false);
                        }
                        catch (Exception ex) {
                            ex.printStackTrace();
                            return Optional.empty();
                        }
                    }
                    return Optional.empty();
                }
            }));

            context.getGlobalObject().set(context, JavaScriptValue.create("run"), JavaScriptJavaCallbackFunctionObject.create(context, "run", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                @Override
                public Optional<JavaScriptValue> callback(JavaScriptValue javaScriptValue, JavaScriptValue[] javaScriptValues) {
                    long sm = System.currentTimeMillis();
                    Optional<JavaScriptString> s = javaScriptValues[0].toString(finalContext);
                    if (s.isPresent()) {
                        Evaluator.evalScript(finalContext, "load('" + s.get().toJavaString() + "')", "<code>", false);
                    }
                    return Optional.of(JavaScriptValue.create(System.currentTimeMillis() - sm));
                }
            }));

            context.getGlobalObject().set(context, JavaScriptValue.create("read"), JavaScriptJavaCallbackFunctionObject.create(context, "read", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                @Override
                public Optional<JavaScriptValue> callback(JavaScriptValue javaScriptValue, JavaScriptValue[] javaScriptValues) {
                    Optional<JavaScriptString> s = javaScriptValues[0].toString(finalContext);
                    if (s.isPresent()) {
                        FileReader in = null;
                        try {
                            in = new FileReader(s.get().toJavaString());
                            char[] chars = new char[(int) (new File(s.get().toJavaString())).length()];
                            in.read(chars);

                            String fileContent = new String(chars);
                            return Optional.of(JavaScriptValue.create(fileContent));
                        }
                        catch (Exception ex) {
                            return Optional.empty();
                        }
                    }
                    return Optional.empty();
                }
            }));
        }

        String source = "load('" + base + "test.js" + "');";
        String fileName = "<java code>";
        Evaluator.evalScript(context,
                source,
                fileName,
                true);
        context = null;
        vm = null;

        Memory.gc();
        Memory.gc();
        Memory.gc();
        Memory.gc();
        Memory.gc();

        Globals.finalizeGlobals();
    }
}