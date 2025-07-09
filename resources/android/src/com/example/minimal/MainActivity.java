package com.example.minimal;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends Activity {
    // Load our native library
    static {
        System.loadLibrary("minimal_native");
    }

    // Declare the native method
    public native String getStringFromJNI();

    @Override
    protected void
    onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        TextView tv = findViewById(R.id.text_view);
        tv.setText(getStringFromJNI());
    }
}
