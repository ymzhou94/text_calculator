package com.textcalculator;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;

public final class MainActivity extends Activity {
    private EditText editor;
    private boolean internalEdit;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        editor = new EditText(this);
        editor.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        editor.setGravity(Gravity.START | Gravity.TOP);
        editor.setTextSize(18);
        editor.setTypeface(Typeface.MONOSPACE);
        editor.setTextColor(Color.rgb(28, 31, 35));
        editor.setBackgroundColor(Color.rgb(250, 250, 250));
        editor.setPadding(28, 28, 28, 28);
        editor.setSingleLine(false);
        editor.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        editor.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        editor.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence text, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence text, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable text) {
                if (!internalEdit) {
                    evaluateAfterEnter(text);
                }
            }
        });

        setContentView(editor);
    }

    private void evaluateAfterEnter(Editable text) {
        int caret = editor.getSelectionStart();
        if (caret <= 0 || caret > text.length()) {
            return;
        }

        EnterInsertion insertion =
                NativeCalculator.buildEnterInsertion(text.subSequence(0, caret).toString());
        if (!insertion.shouldInsert) {
            return;
        }

        internalEdit = true;
        try {
            text.insert(caret, insertion.text);
            editor.setSelection(caret + insertion.text.length());
        } finally {
            internalEdit = false;
        }
    }
}
