package com.samsung.lwe.escargot.util;


import com.samsung.lwe.escargot.Context;
import com.samsung.lwe.escargot.JavaScriptPromiseObject;
import com.samsung.lwe.escargot.JavaScriptValue;
import com.samsung.lwe.escargot.VMInstance;

import java.util.ArrayList;

public class MultiThreadExecutor {
    public static interface WorkerThreadEndNotifier {
        /**
         * called by worker thread before thread exiting
         * @param instance
         */
        public abstract void notify(MultiThreadExecutor executor, ExecutorInstance instance);
    }

    VMInstance m_vmInstance;
    Thread m_javaScriptThread;
    ArrayList<ExecutorInstance> m_activeInstances = new ArrayList<>();
    ArrayList<ExecutorInstance> m_endedInstances = new ArrayList<>();
    WorkerThreadEndNotifier m_workerThreadEndNotifier;

    /**
     * init with current thread as JavaScript Thread
     * and manual threadEndNotifier
     * @param vmInstance
     */
    public MultiThreadExecutor(VMInstance vmInstance)
    {
        this(vmInstance, (executor, instance) -> {
        }, Thread.currentThread());
    }

    /**
     * init with manual threadEndNotifier
     * @param vmInstance
     * @param javaScriptThread
     */
    public MultiThreadExecutor(VMInstance vmInstance, Thread javaScriptThread)
    {
        this(vmInstance, (executor, instance) -> {
        }, javaScriptThread);
    }

    /**
     * init with current thread as JavaScript Thread
     * and manual threadEndNotifier
     * @param vmInstance
     * @param threadEndNotifier
     */
    public MultiThreadExecutor(VMInstance vmInstance, WorkerThreadEndNotifier threadEndNotifier)
    {
        this(vmInstance, threadEndNotifier, Thread.currentThread());
    }

    /**
     * @param vmInstance
     * @param threadEndNotifier
     * @param javaScriptThread
     */
    public MultiThreadExecutor(VMInstance vmInstance, WorkerThreadEndNotifier threadEndNotifier, Thread javaScriptThread)
    {
        if (vmInstance == null || threadEndNotifier == null || javaScriptThread == null) {
            throw new NullPointerException();
        }
        m_javaScriptThread = javaScriptThread;
        m_vmInstance = vmInstance;
        m_workerThreadEndNotifier = threadEndNotifier;
    }

    public static abstract class ResultBuilder {
        public abstract JavaScriptValue build(Context scriptContext, ResultBuilderContext builderContext);
    }

    public static class ResultBuilderContext {
        private boolean m_wasSuccessful;
        private Object m_data;

        public ResultBuilderContext(boolean wasSuccessful, Object data) {
            m_wasSuccessful = wasSuccessful;
            m_data = data;
        }

        public Object data() {
            return m_data;
        }

        public boolean wasSuccessful() {
            return m_wasSuccessful;
        }
    }
    public static abstract class Executor {
        public abstract ResultBuilderContext run();
    }

    public static class ExecutorInstance {
        private Context m_context;
        private JavaScriptPromiseObject m_promise;
        private Thread m_thread;
        private ResultBuilderContext m_builderContext;
        private ResultBuilder m_resultBuilder;

        private ExecutorInstance(Context context, JavaScriptPromiseObject promise,
                         ResultBuilder resultBuilder)
        {
            this.m_context = context;
            this.m_promise = promise;
            this.m_resultBuilder = resultBuilder;
        }

        public Context context() {
            return m_context;
        }

        public JavaScriptPromiseObject promise() {
            return m_promise;
        }

        public Thread thread() {
            return m_thread;
        }

        public ResultBuilderContext builderContext() {
            return m_builderContext;
        }

        public ResultBuilder resultBuilder() {
            return m_resultBuilder;
        }
    }

    /**
     *
     * @param context
     * @param executor
     * @param resultBuilder
     * @return
     */
    public ExecutorInstance startWorker(Context context, Executor executor, ResultBuilder resultBuilder)
    {
        if (Thread.currentThread() != m_javaScriptThread) {
            throw new RuntimeException("You should call this function on JavaScriptThread");
        }
        if (context == null || executor == null || resultBuilder == null) {
            throw new NullPointerException();
        }

        JavaScriptPromiseObject promise = JavaScriptPromiseObject.create(context);

        ExecutorInstance instance = new ExecutorInstance(context, promise, resultBuilder);
        class ExecutorRunnable implements Runnable {
            @Override
            public void run() {
                instance.m_builderContext = executor.run();
                if (instance.m_builderContext == null) {
                    throw new NullPointerException();
                }
                synchronized (m_endedInstances) {
                    m_endedInstances.add(instance);
                }
                synchronized (m_activeInstances) {
                    m_activeInstances.remove(instance);
                }
                m_workerThreadEndNotifier.notify(MultiThreadExecutor.this, instance);
            }
        }

        Thread thread = new Thread(new ExecutorRunnable());
        instance.m_thread = thread;

        synchronized (m_activeInstances) {
            m_activeInstances.add(instance);
        }

        thread.start();

        return instance;
    }

    public boolean hasPendingEvent()
    {
        synchronized (m_endedInstances) {
            return !m_endedInstances.isEmpty();
        }
    }

    public boolean hasPendingWorker()
    {
        synchronized (m_activeInstances) {
            return !m_activeInstances.isEmpty();
        }
    }

    public boolean hasPendingEventOrWorker()
    {
        return hasPendingWorker() || hasPendingEvent();
    }

    /**
     * must call this function from main thread(JS thread)
     */
    public void pumpEventsFromThreadIfNeeds(boolean executePendingJavaScriptJobs)
    {
        if (Thread.currentThread() != m_javaScriptThread) {
            throw new RuntimeException("You should call this function on JavaScriptThread");
        }

        Object[] instances;
        synchronized (m_endedInstances) {
            instances = m_endedInstances.toArray();
            m_endedInstances.clear();
        }

        for (int i = 0; i < instances.length; i ++) {
            ExecutorInstance e = (ExecutorInstance)instances[i];
            JavaScriptValue value = e.resultBuilder().build(e.context(), e.builderContext());
            if (e.builderContext().wasSuccessful()) {
                e.promise().fulfill(e.context(), value);
            } else {
                e.promise().reject(e.context(), value);
            }
        }

        if (executePendingJavaScriptJobs) {
            m_vmInstance.executeEveryPendingJobIfExists();
        }
    }

    /**
     * must call this function from main thread(JS thread)
     */
    public void pumpEventsFromThreadIfNeeds()
    {
        pumpEventsFromThreadIfNeeds(true);
    }
}
