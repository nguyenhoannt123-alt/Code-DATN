package com.example.smartdoorapp;

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.os.Handler;
import android.os.Looper;

import java.net.InetAddress;

public class SmartDoorDiscovery {

    public interface Listener {

        void onSearching();

        void onFound(
                String ip,
                int port
        );

        void onLost();

        void onError(
                String message
        );
    }


    private static final String SERVICE_TYPE =
            "_http._tcp.";

    private static final String EXPECTED_NAME =
            "smartdoor";


    private final NsdManager nsdManager;

    private final Listener listener;

    private final Handler handler =
            new Handler(
                    Looper.getMainLooper()
            );


    private NsdManager.DiscoveryListener discoveryListener;

    private boolean discoveryStarted =
            false;

    private boolean resolveInProgress =
            false;


    public SmartDoorDiscovery(
            Context context,
            Listener listener
    ) {

        this.listener =
                listener;


        nsdManager =
                (NsdManager)
                        context.getSystemService(
                                Context.NSD_SERVICE
                        );
    }


    // =====================================================
    // START
    // =====================================================

    public void start() {

        /*
         * Nếu phiên cũ còn chạy,
         * dừng hẳn rồi mới tạo phiên mới.
         */

        stop();


        handler.postDelayed(
                this::startInternal,
                500
        );
    }


    // =====================================================
    // START INTERNAL
    // =====================================================

    private void startInternal() {

        listener.onSearching();


        resolveInProgress =
                false;


        discoveryListener =
                new NsdManager.DiscoveryListener() {


                    @Override
                    public void onDiscoveryStarted(
                            String serviceType
                    ) {

                        discoveryStarted =
                                true;
                    }


                    // =============================================
                    // FOUND
                    // =============================================

                    @Override
                    public void onServiceFound(
                            NsdServiceInfo serviceInfo
                    ) {

                        if (
                                serviceInfo == null
                        ) {

                            return;
                        }


                        String serviceName =
                                serviceInfo.getServiceName();


                        if (
                                serviceName == null
                        ) {

                            return;
                        }


                        /*
                         * ESP32 đăng ký:
                         *
                         * MDNS_HOSTNAME = smartdoor
                         *
                         * Service name thường chứa smartdoor.
                         */

                        if (
                                !serviceName
                                        .toLowerCase()
                                        .contains(
                                                EXPECTED_NAME
                                        )
                        ) {

                            return;
                        }


                        if (
                                resolveInProgress
                        ) {

                            return;
                        }


                        resolveInProgress =
                                true;


                        resolveService(
                                serviceInfo
                        );
                    }


                    // =============================================
                    // LOST
                    // =============================================

                    @Override
                    public void onServiceLost(
                            NsdServiceInfo serviceInfo
                    ) {

                        resolveInProgress =
                                false;


                        listener.onLost();
                    }


                    // =============================================
                    // STOPPED
                    // =============================================

                    @Override
                    public void onDiscoveryStopped(
                            String serviceType
                    ) {

                        discoveryStarted =
                                false;
                    }


                    // =============================================
                    // START FAILED
                    // =============================================

                    @Override
                    public void onStartDiscoveryFailed(
                            String serviceType,
                            int errorCode
                    ) {

                        discoveryStarted =
                                false;


                        resolveInProgress =
                                false;


                        listener.onError(
                                "Không thể tìm ESP32. Mã lỗi: "
                                        +
                                        errorCode
                        );
                    }


                    // =============================================
                    // STOP FAILED
                    // =============================================

                    @Override
                    public void onStopDiscoveryFailed(
                            String serviceType,
                            int errorCode
                    ) {

                        discoveryStarted =
                                false;
                    }
                };


        try {

            nsdManager.discoverServices(
                    SERVICE_TYPE,
                    NsdManager.PROTOCOL_DNS_SD,
                    discoveryListener
            );
        }

        catch (
                Exception e
        ) {

            discoveryStarted =
                    false;


            resolveInProgress =
                    false;


            listener.onError(
                    "Lỗi NSD: "
                            +
                            e.getMessage()
            );
        }
    }


    // =====================================================
    // RESOLVE
    // =====================================================

    private void resolveService(
            NsdServiceInfo serviceInfo
    ) {

        try {

            nsdManager.resolveService(

                    serviceInfo,

                    new NsdManager.ResolveListener() {


                        @Override
                        public void onResolveFailed(
                                NsdServiceInfo serviceInfo,
                                int errorCode
                        ) {

                            resolveInProgress =
                                    false;


                            /*
                             * Không dừng discovery.
                             *
                             * Cho phép service xuất hiện lại
                             * hoặc thử resolve lại.
                             */

                            listener.onError(
                                    "Tìm thấy Smart Door nhưng chưa lấy được IP. Mã lỗi: "
                                            +
                                            errorCode
                            );
                        }


                        @Override
                        public void onServiceResolved(
                                NsdServiceInfo serviceInfo
                        ) {

                            resolveInProgress =
                                    false;


                            if (
                                    serviceInfo == null
                            ) {

                                listener.onError(
                                        "Thông tin ESP32 không hợp lệ."
                                );

                                return;
                            }


                            InetAddress host =
                                    serviceInfo.getHost();


                            if (
                                    host == null
                            ) {

                                listener.onError(
                                        "Không lấy được IP ESP32."
                                );

                                return;
                            }


                            String ip =
                                    host.getHostAddress();


                            if (
                                    ip == null
                                            ||
                                            ip.trim().isEmpty()
                            ) {

                                listener.onError(
                                        "IP ESP32 rỗng."
                                );

                                return;
                            }


                            int port =
                                    serviceInfo.getPort();


                            if (
                                    port <= 0
                            ) {

                                port = 80;
                            }


                            listener.onFound(
                                    ip,
                                    port
                            );


                            /*
                             * Sau khi tìm được:
                             *
                             * Dừng discovery để không tốn tài nguyên.
                             *
                             * Nếu sau này mất kết nối,
                             * MainActivity sẽ tạo phiên discovery mới.
                             */

                            stop();
                        }
                    }
            );
        }

        catch (
                Exception e
        ) {

            resolveInProgress =
                    false;


            listener.onError(
                    "Lỗi resolve ESP32: "
                            +
                            e.getMessage()
            );
        }
    }


    // =====================================================
    // STOP
    // =====================================================

    public void stop() {

        handler.removeCallbacksAndMessages(
                null
        );


        resolveInProgress =
                false;


        if (
                discoveryListener == null
        ) {

            discoveryStarted =
                    false;

            return;
        }


        if (
                discoveryStarted
        ) {

            try {

                nsdManager.stopServiceDiscovery(
                        discoveryListener
                );
            }

            catch (
                    Exception ignored
            ) {

            }
        }


        discoveryStarted =
                false;


        discoveryListener =
                null;
    }
}