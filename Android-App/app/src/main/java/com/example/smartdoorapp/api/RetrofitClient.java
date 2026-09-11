package com.example.smartdoorapp.api;

import retrofit2.Retrofit;
import retrofit2.converter.gson.GsonConverterFactory;

public class RetrofitClient {

    private static Retrofit retrofit;

    private static SmartDoorApi api;

    private static String currentBaseUrl;


    public static SmartDoorApi getApi(
            String ip,
            int port
    ) {

        String baseUrl =
                "http://"
                        +
                        ip
                        +
                        ":"
                        +
                        port
                        +
                        "/";


        if (
                api == null
                        ||
                        currentBaseUrl == null
                        ||
                        !currentBaseUrl.equals(
                                baseUrl
                        )
        ) {

            currentBaseUrl =
                    baseUrl;


            retrofit =
                    new Retrofit.Builder()
                            .baseUrl(
                                    baseUrl
                            )
                            .addConverterFactory(
                                    GsonConverterFactory.create()
                            )
                            .build();


            api =
                    retrofit.create(
                            SmartDoorApi.class
                    );
        }


        return api;
    }


    public static void reset() {

        retrofit = null;

        api = null;

        currentBaseUrl = null;
    }


    public static String getCurrentBaseUrl() {

        return currentBaseUrl;
    }
}