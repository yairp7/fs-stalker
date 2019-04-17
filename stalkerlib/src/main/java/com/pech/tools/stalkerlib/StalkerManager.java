package com.pech.tools.stalkerlib;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

public class StalkerManager {
    public enum eEventType {
        ACCESS("IN_ACCESS".hashCode()),
        MODIFY("IN_MODIFY".hashCode()),
        CREATE("IN_CREATE".hashCode()),
        DELETE("IN_DELETE".hashCode());

        private int value;
        eEventType(int value) {
            this.value = value;
        }

        public int getValue() {
            return value;
        }

        public static eEventType getEventTypeById(int id) {
            for(eEventType type : eEventType.values()) {
                if(type.getValue() == id) {
                    return type;
                }
            }
            return null;
        }
    }

    private final String EXEC_NAME = "fs-stalker";
    private final String LIB_NAME = "libstalker.so";

    private HandlerThread mDataThread = null;
    private Handler mDataHandler = null;
    private ExecRunnable mExecRunnable = null;

    private IEventListener mListener = null;

    private boolean mVerbose = false;
    private String mOutputFile = null;

    public StalkerManager(IEventListener eventListener) {
        mListener = eventListener;
    }

    public void setup(Context context) {
        copyAndSetExecutable(context, EXEC_NAME);
        copyAndSetExecutable(context, LIB_NAME);
    }

    public void run(Context context, boolean verbose) throws IOException {
        String execPath = String.format("./%s", EXEC_NAME);
        String mainPath = String.format("/data/data/%s", context.getPackageName());
        String params = mainPath;
        if(mOutputFile != null) {
            addParam(params, "-o", mOutputFile);
        }
        String cmd = String.format("%s %s", execPath, params);
        Process process = Runtime.getRuntime().exec(cmd, null, context.getFilesDir());
        log("Running Command: " + cmd);

        mVerbose = verbose;

        mDataThread = new HandlerThread("DataThread");
        mDataThread.start();
        mDataHandler = new Handler(mDataThread.getLooper());
        mExecRunnable = new ExecRunnable(process);
        mDataHandler.post(mExecRunnable);
    }

    public void setOutputFile(String outputFile) {
        mOutputFile = outputFile;
    }

    private String addParam(String params, String paramKey, String paramValue) {
        return String.format("%s %s %s", paramKey, paramValue, params);
    }

    private void copyAndSetExecutable(Context context, String filename) {
        String cpuArch = Build.CPU_ABI;
        String srcPath = String.format("%s/%s", cpuArch, filename);
        String destPath = String.format("%s", filename);
        try {
            InputStream is = context.getAssets().open(srcPath);
            byte[] buffer = new byte[is.available()];
            is.read(buffer);
            is.close();

            log("Copying from: " + srcPath);

            FileOutputStream os = context.openFileOutput(destPath, Context.MODE_PRIVATE);
            os.write(buffer);
            os.flush();
            os.close();

            log("Copying to: " + destPath);

            File file = context.getFileStreamPath(destPath);
            file.setExecutable(true);

            log("Setting executable: " + destPath);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private class ExecRunnable implements Runnable {
        private volatile boolean mIsRunning = true;
        private InputStream mInputStreamErr;
        private byte[] buffer = new byte[1024];

        public ExecRunnable(Process process) {
            mInputStreamErr = process.getErrorStream();
        }

        @Override
        public void run() {
            try {
                while (mIsRunning) {
                    while (mInputStreamErr.available() > 0) {
                        int index = 0;
                        while(mInputStreamErr.available() > 0) {
                            char b = (char) mInputStreamErr.read();
                            buffer[index] = (byte) b;
                            if(b == '\n') {
                                notify(Arrays.copyOf(buffer, index));
                                Arrays.fill(buffer, 0, index, (byte) 0);
                                index = 0;
                            }
                            else {
                                index++;
                            }
                        }
                    }
                }
            }
            catch (IOException e) {
                e.printStackTrace();
            }
        }

        private void notify(byte[] buffer) {
            if(mListener != null) {
                String msg = new String(buffer);
                log(msg);
                String[] parts = msg.substring(4).split("\\|");

                if(parts.length > 1) {
                    mListener.onEvent(Long.parseLong(parts[0]), parts[1], eEventType.getEventTypeById(parts[2].hashCode()));
                }
            }
        }

//        private FSEvent processEvent(String msg) {
//            String[] msgParts = msg.split("|");
//            if(msgParts.length > 1) {
//                FSEvent event;
//                int eventHash = msgParts[1].hashCode() + msgParts[2].hashCode();
//                if(mEventsMap.indexOfKey(eventHash) >= 0) {
//                    event = mEventsMap.get(eventHash).increment();
//                }
//                else {
//                    event = new FSEvent(msgParts[1], msgParts[2]);
//                    mEventsMap.put(eventHash, event);
//                }
//                return event;
//            }
//            return null;
//        }

//        private void handleMsg(final String msg) {
//            mEventsCounter++;
//            FSEvent event = processEvent(msg);
//            if(event != null) {
//                System.out.println("File updated: " + event.mPath);
//            }
//            if(mEventsCounter > LINES_BUFFER_SIZE) {
//                final StringBuilder sb = new StringBuilder();
//                for(int i = 0; i < mEventsMap.size(); i++) {
//                    Integer key = mEventsMap.keyAt(i);
//                    event = mEventsMap.get(key);
//                    sb.append(String.format(Locale.getDefault(), "%s:%s[%d]\n", event.mPath, event.mEventType, event.mCount));
//                }
//                runOnUiThread(new Runnable() {
//                    @Override
//                    public void run() {
//                        mMsgsTV.setText(sb.toString());
//                    }
//                });
//                mEventsCounter = 0;
//            }
//        }

        void stop() {
            mIsRunning = false;
        }
    }

    public void onDestroy() {
        if(mExecRunnable != null) {
            mExecRunnable.stop();
        }

        if(mDataThread != null && mDataThread.isAlive()) {
            mDataThread.interrupt();
        }
    }

    private void log(String msg) {
        if(mVerbose) {
            System.out.println(msg);
        }
    }

    public interface IEventListener {
        void onEvent(long timestamp, String path, eEventType type);
    }
}
