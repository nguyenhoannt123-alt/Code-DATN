package com.example.smartdoorapp.model;

import java.util.List;

public class HistoryResponse {

    private boolean success;

    private int count;

    private List<HistoryItem> history;

    public boolean isSuccess() {
        return success;
    }

    public int getCount() {
        return count;
    }

    public List<HistoryItem> getHistory() {
        return history;
    }
}