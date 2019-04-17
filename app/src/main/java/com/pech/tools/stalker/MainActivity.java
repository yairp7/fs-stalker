package com.pech.tools.stalker;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.text.method.ScrollingMovementMethod;
import android.util.SparseArray;
import android.widget.TextView;

import com.pech.tools.stalkerlib.StalkerManager;

import java.io.IOException;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "StalkerApp";

    private StalkerManager mStalkerManager = null;

    private TextView mMsgsTV = null;

    private SparseArray<FSEvent> mEventsMap = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mMsgsTV = findViewById(R.id.msgs);
        mMsgsTV.setMovementMethod(new ScrollingMovementMethod());

        mStalkerManager = new StalkerManager(new StalkerManager.IEventListener() {
            @Override
            public void onEvent(long timestamp, final String path, StalkerManager.eEventType type) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mMsgsTV.append(path + "\n");
                    }
                });
            }
        });
        mStalkerManager.setup(getApplicationContext());

        runBinary();
    }

    private void runBinary() {
        try {
            mStalkerManager.setOutputFile("out.txt");
            mStalkerManager.run(getApplicationContext(), true);
        } catch (IOException e) {
            e.printStackTrace();
        }
        mEventsMap = new SparseArray<>();
    }

    @Override
    protected void onDestroy() {
        if(mStalkerManager != null) {
            mStalkerManager.onDestroy();
        }
        super.onDestroy();
    }

    class FSEvent {
        FSEvent(String path, String eventType) {
            mPath = path;
            mEventType = eventType;
            mCount = 1;
        }

        String mPath;
        String mEventType;
        int mCount;

        FSEvent increment() {
            mCount++;
            return this;
        }
    }
}
