package com.example.smartdoorapp.model;

public class CountResponse {

    private boolean success;
    private int count;
    private String message;

    public boolean isSuccess() {
        return success;
    }

    public int getCount() {
        return count;
    }

    public String getMessage() {
        return message;
    }
}