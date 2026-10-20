package com.frmod;

public final class Native {
    static {
        System.loadLibrary("frmod");
    }

    private Native() {}

    /** 0 thick smoke | 1 smoke duration | 2 car no damage | 3 tire no damage | 4 speed % | 5 fps | -1 so carrega a lib */
    public static native void setFlag(int id, int value);
}
