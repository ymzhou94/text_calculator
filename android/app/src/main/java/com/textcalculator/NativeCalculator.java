package com.textcalculator;

public final class NativeCalculator {
    static {
        System.loadLibrary("textcalc_jni");
    }

    private NativeCalculator() {
    }

    public static native EvalResult evaluate(String expression);

    public static native EnterInsertion buildEnterInsertion(String textBeforeCaret);
}
