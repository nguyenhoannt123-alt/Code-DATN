package com.example.smartdoorapp.model;

public class BasicResponse {

    private boolean success;
    private String message;
    private Integer id;
    private Integer count;

    public boolean isSuccess() {
        return success;
    }

    public String getMessage() {
        return message;
    }

    public Integer getId() {
        return id;
    }

    public Integer getCount() {
        return count;
    }
}