package com.textcalculator;

public final class EvalResult {
    public final boolean ok;
    public final String output;

    public EvalResult(boolean ok, String output) {
        this.ok = ok;
        this.output = output;
    }
}
