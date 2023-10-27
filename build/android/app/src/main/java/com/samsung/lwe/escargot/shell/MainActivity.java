package com.samsung.lwe.escargot.shell;

import androidx.appcompat.app.AppCompatActivity;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;

import com.samsung.lwe.escargot.Bridge;
import com.samsung.lwe.escargot.Context;
import com.samsung.lwe.escargot.Evaluator;
import com.samsung.lwe.escargot.Globals;
import com.samsung.lwe.escargot.JavaScriptArrayObject;
import com.samsung.lwe.escargot.JavaScriptJavaCallbackFunctionObject;
import com.samsung.lwe.escargot.JavaScriptString;
import com.samsung.lwe.escargot.JavaScriptValue;
import com.samsung.lwe.escargot.Memory;
import com.samsung.lwe.escargot.VMInstance;
import com.samsung.lwe.escargot.util.MultiThreadExecutor;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
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

    String str = "";
    private void run()
    {
        final String base = getApplicationContext().getFilesDir().getAbsolutePath() + "/";
        new Thread(() -> {
            Globals.initializeGlobals();
            Memory.setGCFrequency(2);

            VMInstance vm = VMInstance.create(Optional.empty(), Optional.empty());
            Context context = Context.create(vm);

            {
                Context finalContext = context;
                context.getGlobalObject().set(context, JavaScriptString.create("print"), JavaScriptJavaCallbackFunctionObject.create(context, "print", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue javaScriptValue, JavaScriptValue[] javaScriptValues) {
                        StringBuffer sb = new StringBuffer();
                        sb.append(str);
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

                        str = sb.toString();
                        final String ff = str;

                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                TextView tv = ((TextView)findViewById(R.id.output));
                                tv.setText(ff);
                            }
                        });

                        return Optional.empty();
                    }
                }));

                context.getGlobalObject().set(context, JavaScriptString.create("load"), JavaScriptJavaCallbackFunctionObject.create(context, "run", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue javaScriptValue, JavaScriptValue[] javaScriptValues) {
                        Optional<JavaScriptString> s = javaScriptValues[0].toString(finalContext);
                        if (s.isPresent()) {
                            try {
                                byte[] chars = Files.readAllBytes(Paths.get(s.get().toJavaString()));
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

                context.getGlobalObject().set(context, JavaScriptString.create("run"), JavaScriptJavaCallbackFunctionObject.create(context, "run", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue javaScriptValue, JavaScriptValue[] javaScriptValues) {
                        long sm = System.currentTimeMillis();
                        Optional<JavaScriptString> s = javaScriptValues[0].toString(finalContext);
                        if (s.isPresent()) {
                            Evaluator.evalScript(finalContext, "load('" + s.get().toJavaString() + "')", "<code>", false);
                        }
                        return Optional.of(JavaScriptValue.create(System.currentTimeMillis() - sm));
                    }
                }));

                context.getGlobalObject().set(context, JavaScriptString.create("read"), JavaScriptJavaCallbackFunctionObject.create(context, "read", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue javaScriptValue, JavaScriptValue[] javaScriptValues) {
                        Optional<JavaScriptString> s = javaScriptValues[0].toString(finalContext);
                        if (s.isPresent()) {
                            FileReader in = null;
                            try {
                                byte[] chars = Files.readAllBytes(Paths.get(s.get().toJavaString()));
                                String fileContent = new String(chars);
                                return Optional.of(JavaScriptString.create(fileContent));
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
        }).start();
    }

    public void runExample()
    {
        new Thread(() -> {
            Looper.prepare();
            Handler handler = new Handler(Looper.myLooper());

            Globals.initializeGlobals();

            VMInstance vmInstance = VMInstance.create(Optional.empty(), Optional.empty());
            Context context = Context.create(vmInstance);

            context.getGlobalObject().set(context, JavaScriptString.create("print"), JavaScriptJavaCallbackFunctionObject.create(context, "", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                @Override
                public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                    System.out.println(arguments[0].toString(context).get().toJavaString());
                    return Optional.empty();
                }
            }));

            context.getGlobalObject().set(context, JavaScriptString.create("end"), JavaScriptJavaCallbackFunctionObject.create(context, "", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                @Override
                public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                    Looper.myLooper().quitSafely();
                    return Optional.empty();
                }
            }));

            MultiThreadExecutor executor = new MultiThreadExecutor(vmInstance, new MultiThreadExecutor.WorkerThreadEndNotifier() {
                @Override
                public void notify(MultiThreadExecutor executor, MultiThreadExecutor.ExecutorInstance instance) {
                    handler.post(new Runnable() {
                        @Override
                        public void run() {
                            executor.pumpEventsFromThreadIfNeeds();
                        }
                    });
                }
            });
            Bridge.register(context, "HTTP", "get", new Bridge.Adapter() {
                @Override
                public Optional<JavaScriptValue> callback(Context context, Optional<JavaScriptValue> data) {
                    String url = "";
                    if (data.isPresent()) {
                        Optional<JavaScriptString> mayBeString = data.get().toString(context);
                        if (mayBeString.isPresent()) {
                            url = mayBeString.get().toJavaString();
                        }
                    }
                    final String finalURL = url;
                    MultiThreadExecutor.ExecutorInstance instance = executor.startWorker(context, new MultiThreadExecutor.Executor() {
                        @Override
                        public MultiThreadExecutor.ResultBuilderContext run() {
                            try {
                                HttpURLConnection connection = (HttpURLConnection)new URL(finalURL).openConnection();
                                connection.setRequestMethod("GET");
                                int responseCode = connection.getResponseCode();
                                if (responseCode == HttpURLConnection.HTTP_OK) { // success
                                    BufferedReader in = new BufferedReader(new InputStreamReader(connection.getInputStream()));
                                    String inputLine;
                                    StringBuffer response = new StringBuffer();

                                    while ((inputLine = in.readLine()) != null) {
                                        response.append(inputLine);
                                    }
                                    in.close();

                                    return new MultiThreadExecutor.ResultBuilderContext(true, response.toString());
                                } else {
                                    return new MultiThreadExecutor.ResultBuilderContext(false, "error HTTP return code:" + responseCode);
                                }
                            } catch (IOException e) {
                                return new MultiThreadExecutor.ResultBuilderContext(false, e.toString());
                            }
                        }
                    }, new MultiThreadExecutor.ResultBuilder() {
                        @Override
                        public JavaScriptValue build(Context scriptContext, MultiThreadExecutor.ResultBuilderContext builderContext) {
                            return JavaScriptString.create((String)builderContext.data());
                        }
                    });
                    return Optional.of(instance.promise());
                }
            });

            Evaluator.evalScript(context, "" +
                    "let promise1 = HTTP.get('https://httpbin.org/get');" +
                    "let promise2 = HTTP.get('http://google.com');" +
                    "Promise.allSettled([promise1, promise2]).then(function(v) {" +
                    "print(JSON.stringify(v));" +
                    "print('http all end!');" +
                    "end();" +
                    "});", "", false);


            Looper.loop();
            Looper.myLooper().quit();

            context = null;
            vmInstance = null;

            Memory.gc();
            Memory.gc();
            Memory.gc();
            Memory.gc();
            Memory.gc();

            Globals.finalizeGlobals();
        }).start();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // copy assets to internal storage (for copying js files which are used by test)
        copyAssets();

        findViewById(R.id.button).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                run();
            }
        });

        findViewById(R.id.button2).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                runExample();
            }
        });
    }
}