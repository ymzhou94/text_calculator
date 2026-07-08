package com.textcalculator;

public final class EnterInsertion {
    public final boolean shouldInsert;
    public final String text;

    public EnterInsertion(boolean shouldInsert, String text) {
        this.shouldInsert = shouldInsert;
        this.text = text;
    }
}
