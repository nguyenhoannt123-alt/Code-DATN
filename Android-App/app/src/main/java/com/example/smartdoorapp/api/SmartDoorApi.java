package com.example.smartdoorapp.api;

import com.example.smartdoorapp.model.BasicResponse;
import com.example.smartdoorapp.model.CountResponse;
import com.example.smartdoorapp.model.HistoryResponse;
import com.example.smartdoorapp.model.StatusResponse;

import retrofit2.Call;
import retrofit2.http.GET;
import retrofit2.http.POST;
import retrofit2.http.Query;

public interface SmartDoorApi {

    @GET("api/status")
    Call<StatusResponse> getStatus();

    @POST("api/door/open")
    Call<BasicResponse> openDoor();

    @POST("api/door/lock")
    Call<BasicResponse> lockDoor();

    @POST("api/fingerprint/enroll")
    Call<BasicResponse> enrollFingerprint(@Query("id") int id);

    @POST("api/fingerprint/delete")
    Call<BasicResponse> deleteFingerprint(@Query("id") int id);

    @GET("api/fingerprint/count")
    Call<CountResponse> getFingerprintCount();

    @GET("api/history")
    Call<HistoryResponse> getHistory();
}