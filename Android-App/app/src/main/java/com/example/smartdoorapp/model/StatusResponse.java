package com.example.smartdoorapp.model;

public class StatusResponse {

    private boolean success;

    private boolean doorLocked;

    private boolean fingerprintConnected;

    private boolean rfidConnected;

    private boolean wifiConnected;

    private String ip;

    public boolean isSuccess() {
        return success;
    }

    public boolean isDoorLocked() {
        return doorLocked;
    }

    public boolean isFingerprintConnected() {
        return fingerprintConnected;
    }

    public boolean isRfidConnected() {
        return rfidConnected;
    }

    public boolean isWifiConnected() {
        return wifiConnected;
    }

    public String getIp() {
        return ip;
    }
}