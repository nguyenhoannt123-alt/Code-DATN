package com.example.smartdoorapp;

import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.example.smartdoorapp.api.RetrofitClient;
import com.example.smartdoorapp.api.SmartDoorApi;
import com.example.smartdoorapp.model.BasicResponse;
import com.example.smartdoorapp.model.CountResponse;
import com.example.smartdoorapp.model.HistoryItem;
import com.example.smartdoorapp.model.HistoryResponse;
import com.example.smartdoorapp.model.StatusResponse;

import org.json.JSONObject;

import java.util.List;

import okhttp3.ResponseBody;

import retrofit2.Call;
import retrofit2.Callback;
import retrofit2.Response;


// =========================================================
// MAIN ACTIVITY
// =========================================================

public class MainActivity extends AppCompatActivity {


    // =====================================================
    // VIEW
    // =====================================================

    private EditText edtEspIp;

    private Button btnSaveIp;

    private EditText edtFingerprintId;

    private Button btnOpenDoor;

    private Button btnLockDoor;

    private Button btnEnrollFingerprint;

    private Button btnDeleteFingerprint;

    private Button btnRefresh;

    private TextView tvConnection;

    private TextView tvIp;

    private TextView tvDoorStatus;

    private TextView tvDoorIcon;

    private LinearLayout cardDoor;

    private TextView tvFingerprintStatus;

    private TextView tvRfidStatus;

    private TextView tvFingerprintCount;

    private TextView tvMessage;

    private LinearLayout layoutHistory;


    // =====================================================
    // API
    // =====================================================

    private SmartDoorApi api;


    // =====================================================
    // SMART DOOR DISCOVERY
    // =====================================================

    private SmartDoorDiscovery smartDoorDiscovery;

    private String currentEspIp = null;

    private int currentEspPort = 80;

    private boolean discoveryInProgress = false;

    // =====================================================
    // KET NOI TRUC TIEP VOI WIFI DO ESP32 PHAT
    // =====================================================

    // Khi dien thoai ket noi WiFi "SmartDoor_Direct",
    // ESP32 SoftAP luon co dia chi nay.
    private static final String DIRECT_AP_IP =
            "192.168.4.1";

    private static final int DIRECT_AP_PORT =
            80;

    // Tranh gui nhieu request thu 192.168.4.1 cung luc.
    private boolean directProbeInProgress =
            false;


    // =====================================================
    // HANDLER RECONNECT
    // =====================================================

    private final Handler reconnectHandler =
            new Handler(
                    Looper.getMainLooper()
            );


    private final Runnable reconnectRunnable =
            new Runnable() {

                @Override
                public void run() {

                    if (
                            isFinishing()
                                    ||
                                    isDestroyed()
                    ) {

                        return;
                    }


                    if (
                            api == null
                    ) {

                        startSmartDoorDiscovery();
                    }
                }
            };


    // =====================================================
    // AUTO REFRESH
    // =====================================================

    private static final long AUTO_REFRESH_INTERVAL =
            500L;


    private final Handler autoRefreshHandler =
            new Handler(
                    Looper.getMainLooper()
            );


    private boolean autoRefreshEnabled = false;

    // Lich su cap nhat cham hon trang thai cua de tranh gui qua nhieu request.
    private int historyRefreshCounter = 0;


    private final Runnable autoRefreshRunnable =
            new Runnable() {

                @Override
                public void run() {

                    if (
                            !autoRefreshEnabled
                    ) {

                        return;
                    }


                    if (
                            api == null
                    ) {

                        if (
                                !discoveryInProgress
                        ) {

                            startSmartDoorDiscovery();
                        }
                    }

                    else {

                        // Trang thai cua cap nhat moi 500 ms.
                        loadStatus();

                        // Lich su cap nhat moi 1 giay.
                        historyRefreshCounter++;

                        if (
                                historyRefreshCounter >= 2
                        ) {

                            historyRefreshCounter = 0;
                            loadHistory();
                        }
                    }


                    autoRefreshHandler.postDelayed(
                            this,
                            AUTO_REFRESH_INTERVAL
                    );
                }
            };


    // =====================================================
    // ON CREATE
    // =====================================================

    @Override
    protected void onCreate(
            Bundle savedInstanceState
    ) {

        super.onCreate(
                savedInstanceState
        );


        // =================================================
        // LOAD LAYOUT
        // =================================================

        setContentView(
                R.layout.activity_main
        );


        // =================================================
        // MAP VIEW
        // =================================================

        edtEspIp =
                findViewById(
                        R.id.edtEspIp
                );


        btnSaveIp =
                findViewById(
                        R.id.btnSaveIp
                );


        tvConnection =
                findViewById(
                        R.id.tvConnection
                );


        tvIp =
                findViewById(
                        R.id.tvIp
                );


        tvDoorStatus =
                findViewById(
                        R.id.tvDoorStatus
                );


        tvDoorIcon =
                findViewById(
                        R.id.tvDoorIcon
                );


        cardDoor =
                findViewById(
                        R.id.cardDoor
                );


        tvFingerprintStatus =
                findViewById(
                        R.id.tvFingerprintStatus
                );


        tvRfidStatus =
                findViewById(
                        R.id.tvRfidStatus
                );


        tvFingerprintCount =
                findViewById(
                        R.id.tvFingerprintCount
                );


        tvMessage =
                findViewById(
                        R.id.tvMessage
                );


        edtFingerprintId =
                findViewById(
                        R.id.edtFingerprintId
                );


        btnOpenDoor =
                findViewById(
                        R.id.btnOpenDoor
                );


        btnLockDoor =
                findViewById(
                        R.id.btnLockDoor
                );


        btnEnrollFingerprint =
                findViewById(
                        R.id.btnEnrollFingerprint
                );


        btnDeleteFingerprint =
                findViewById(
                        R.id.btnDeleteFingerprint
                );


        btnRefresh =
                findViewById(
                        R.id.btnRefresh
                );


        layoutHistory =
                findViewById(
                        R.id.layoutHistory
                );


        // =================================================
        // ẨN PHẦN NHẬP IP CŨ
        // =================================================

        if (
                edtEspIp != null
        ) {

            edtEspIp.setVisibility(
                    View.GONE
            );
        }


        if (
                btnSaveIp != null
        ) {

            btnSaveIp.setVisibility(
                    View.GONE
            );
        }


        // =================================================
        // INIT
        // =================================================

        api = null;

        currentEspIp = null;

        currentEspPort = 80;


        showConnectionSearching();


        tvIp.setText(
                "IP ESP32: Đang tự động tìm..."
        );


        tvDoorStatus.setText(
                "Cửa: Đang kiểm tra..."
        );


        tvFingerprintStatus.setText(
                "AS608: Đang kiểm tra..."
        );


        tvRfidStatus.setText(
                "RC522: Đang kiểm tra..."
        );


        tvFingerprintCount.setText(
                "Số mẫu vân tay đang lưu: --"
        );


        tvMessage.setText(
                "Đang tìm Smart Door trong mạng..."
        );


        // =================================================
        // BUTTON
        // =================================================

        btnOpenDoor.setOnClickListener(
                v -> openDoor()
        );


        btnLockDoor.setOnClickListener(
                v -> lockDoor()
        );


        btnEnrollFingerprint.setOnClickListener(
                v -> enrollFingerprint()
        );


        btnDeleteFingerprint.setOnClickListener(
                v -> deleteFingerprint()
        );


        btnRefresh.setOnClickListener(
                v -> {

                    if (
                            api == null
                    ) {

                        tvMessage.setText(
                                "Đang tìm lại ESP32..."
                        );


                        restartDiscoveryDelayed(
                                300
                        );
                    }

                    else {

                        tvMessage.setText(
                                "Đang làm mới dữ liệu..."
                        );


                        refreshAll();
                    }
                }
        );


        // =================================================
        // DISCOVERY FIRST TIME
        // =================================================

        restartDiscoveryDelayed(
                300
        );
    }


    // =====================================================
    // ON RESUME
    // =====================================================

    @Override
    protected void onResume() {

        super.onResume();


        startAutoRefresh();


        if (
                api == null
        ) {

            restartDiscoveryDelayed(
                    500
            );
        }
    }


    // =====================================================
    // ON PAUSE
    // =====================================================

    @Override
    protected void onPause() {

        super.onPause();


        stopAutoRefresh();


        reconnectHandler.removeCallbacks(
                reconnectRunnable
        );


        stopDiscovery();
    }


    // =====================================================
    // ON DESTROY
    // =====================================================

    @Override
    protected void onDestroy() {

        stopAutoRefresh();


        reconnectHandler.removeCallbacksAndMessages(
                null
        );


        stopDiscovery();


        super.onDestroy();
    }


    // =====================================================
    // AUTO REFRESH
    // =====================================================

    private void startAutoRefresh() {

        if (
                autoRefreshEnabled
        ) {

            return;
        }


        autoRefreshEnabled =
                true;


        autoRefreshHandler.removeCallbacks(
                autoRefreshRunnable
        );


        autoRefreshHandler.postDelayed(
                autoRefreshRunnable,
                AUTO_REFRESH_INTERVAL
        );
    }


    private void stopAutoRefresh() {

        autoRefreshEnabled =
                false;


        autoRefreshHandler.removeCallbacks(
                autoRefreshRunnable
        );
    }


    // =====================================================
    // RESTART DISCOVERY DELAYED
    // =====================================================

    private void restartDiscoveryDelayed(
            long delayMs
    ) {

        reconnectHandler.removeCallbacks(
                reconnectRunnable
        );


        stopDiscovery();


        discoveryInProgress =
                false;


        reconnectHandler.postDelayed(
                reconnectRunnable,
                delayMs
        );
    }


    // =====================================================
    // THU KET NOI TRUC TIEP 192.168.4.1
    // =====================================================

    private void tryDirectEspConnection() {

        if (
                api != null
                        ||
                        directProbeInProgress
        ) {

            return;
        }


        directProbeInProgress =
                true;


        /*
         * Khong gan ngay vao bien api.
         * Chi khi /api/status cua 192.168.4.1 tra loi thanh cong
         * moi coi day la ESP32 cua he thong.
         */
        RetrofitClient.reset();

        final SmartDoorApi directApi =
                RetrofitClient.getApi(
                        DIRECT_AP_IP,
                        DIRECT_AP_PORT
                );


        directApi.getStatus().enqueue(

                new Callback<StatusResponse>() {

                    @Override
                    public void onResponse(
                            Call<StatusResponse> call,
                            Response<StatusResponse> response
                    ) {

                        directProbeInProgress =
                                false;


                        // Neu NSD da tim duoc ESP32 truoc thi giu ket noi do.
                        if (
                                api != null
                        ) {

                            return;
                        }


                        if (
                                response.isSuccessful()
                                        &&
                                        response.body() != null
                        ) {

                            stopDiscovery();

                            discoveryInProgress =
                                    false;


                            reconnectHandler.removeCallbacks(
                                    reconnectRunnable
                            );


                            currentEspIp =
                                    DIRECT_AP_IP;

                            currentEspPort =
                                    DIRECT_AP_PORT;


                            /*
                             * Tao lai API chinh thuc cho ket noi SoftAP.
                             */
                            RetrofitClient.reset();

                            api =
                                    RetrofitClient.getApi(
                                            currentEspIp,
                                            currentEspPort
                                    );


                            showConnectionSearching();


                            tvIp.setText(
                                    "IP ESP32: "
                                            +
                                            DIRECT_AP_IP
                                            +
                                            " (WiFi ESP32)"
                            );


                            tvMessage.setText(
                                    "Đã kết nối trực tiếp SmartDoor_Direct."
                            );


                            refreshAll();
                        }
                    }


                    @Override
                    public void onFailure(
                            Call<StatusResponse> call,
                            Throwable t
                    ) {

                        /*
                         * 192.168.4.1 khong truy cap duoc:
                         * khong bao loi ngay.
                         *
                         * NSD van dang chay song song de tim ESP32
                         * neu dien thoai va ESP32 dang o cung WiFi ngoai.
                         */
                        directProbeInProgress =
                                false;
                    }
                }
        );
    }


    // =====================================================
    // START DISCOVERY
    // =====================================================

    private void startSmartDoorDiscovery() {

        if (
                api != null
        ) {

            return;
        }


        if (
                discoveryInProgress
        ) {

            return;
        }


        discoveryInProgress =
                true;


        /*
         * Thu duong truc tiep cua SoftAP truoc/song song.
         * Neu dien thoai dang bat SmartDoor_Direct,
         * dia chi 192.168.4.1 se tra loi ngay.
         */
        tryDirectEspConnection();


        showConnectionSearching();


        tvIp.setText(
                "IP ESP32: Đang tìm..."
        );


        tvMessage.setText(
                "Đang tự động tìm Smart Door..."
        );


        if (
                smartDoorDiscovery != null
        ) {

            smartDoorDiscovery.stop();

            smartDoorDiscovery = null;
        }


        smartDoorDiscovery =
                new SmartDoorDiscovery(

                        this,

                        new SmartDoorDiscovery.Listener() {


                            // =========================================
                            // SEARCHING
                            // =========================================

                            @Override
                            public void onSearching() {

                                runOnUiThread(
                                        () -> {

                                            discoveryInProgress =
                                                    true;


                                            showConnectionSearching();


                                            tvMessage.setText(
                                                    "Đang tìm ESP32 trong mạng hotspot..."
                                            );
                                        }
                                );
                            }


                            // =========================================
                            // FOUND
                            // =========================================

                            @Override
                            public void onFound(
                                    String ip,
                                    int port
                            ) {

                                runOnUiThread(
                                        () -> {

                                            // Neu da ket noi truc tiep 192.168.4.1
                                            // thi khong de NSD ghi de ket noi.
                                            if (
                                                    api != null
                                                            &&
                                                            DIRECT_AP_IP.equals(
                                                                    currentEspIp
                                                            )
                                            ) {

                                                return;
                                            }


                                            discoveryInProgress =
                                                    false;


                                            reconnectHandler.removeCallbacks(
                                                    reconnectRunnable
                                            );


                                            currentEspIp =
                                                    ip;


                                            currentEspPort =
                                                    port > 0
                                                            ? port
                                                            : 80;


                                            RetrofitClient.reset();


                                            api =
                                                    RetrofitClient.getApi(
                                                            currentEspIp,
                                                            currentEspPort
                                                    );


                                            showConnectionSearching();


                                            tvIp.setText(
                                                    "IP ESP32: "
                                                            +
                                                            currentEspIp
                                            );


                                            tvMessage.setText(
                                                    "Đã tìm thấy Smart Door. Đang kiểm tra API..."
                                            );


                                            refreshAll();
                                        }
                                );
                            }


                            // =========================================
                            // LOST
                            // =========================================

                            @Override
                            public void onLost() {

                                runOnUiThread(
                                        () -> {

                                            // NSD co the bao LOST khi ta dung discovery,
                                            // nhung neu dang noi truc tiep SoftAP thi bo qua.
                                            if (
                                                    DIRECT_AP_IP.equals(
                                                            currentEspIp
                                                    )
                                            ) {

                                                return;
                                            }


                                            if (
                                                    api == null
                                                            &&
                                                            currentEspIp == null
                                            ) {

                                                return;
                                            }


                                            showConnectionOffline();


                                            tvIp.setText(
                                                    "IP ESP32: --"
                                            );


                                            tvMessage.setText(
                                                    "ESP32 đã rời mạng. Đang chờ kết nối lại..."
                                            );


                                            currentEspIp =
                                                    null;


                                            currentEspPort =
                                                    80;


                                            RetrofitClient.reset();


                                            api =
                                                    null;


                                            discoveryInProgress =
                                                    false;


                                            /*
                                             * Không discovery ngay.
                                             *
                                             * Đợi Android dựng lại network.
                                             */
                                            restartDiscoveryDelayed(
                                                    2000
                                            );
                                        }
                                );
                            }


                            // =========================================
                            // ERROR
                            // =========================================

                            @Override
                            public void onError(
                                    String message
                            ) {

                                runOnUiThread(
                                        () -> {

                                            discoveryInProgress =
                                                    false;


                                            showConnectionOffline();


                                            tvIp.setText(
                                                    "IP ESP32: --"
                                            );


                                            tvMessage.setText(
                                                    message
                                                            +
                                                            "\nApp sẽ tự tìm lại."
                                            );


                                            /*
                                             * Nếu discovery lỗi,
                                             * tạo phiên mới sau 2 giây.
                                             */
                                            restartDiscoveryDelayed(
                                                    2000
                                            );
                                        }
                                );
                            }
                        }
                );


        smartDoorDiscovery.start();
    }


    // =====================================================
    // STOP DISCOVERY
    // =====================================================

    private void stopDiscovery() {

        discoveryInProgress =
                false;


        if (
                smartDoorDiscovery != null
        ) {

            smartDoorDiscovery.stop();

            smartDoorDiscovery = null;
        }
    }


    // =====================================================
    // CONNECTION LOST
    // =====================================================

    private void handleConnectionLost(
            String message
    ) {

        showConnectionOffline();


        tvIp.setText(
                "IP ESP32: --"
        );


        tvMessage.setText(
                message
        );


        currentEspIp =
                null;


        currentEspPort =
                80;


        RetrofitClient.reset();


        api =
                null;


        directProbeInProgress =
                false;

        discoveryInProgress =
                false;


        /*
         * QUAN TRỌNG:
         *
         * Dừng phiên NSD cũ.
         * Chờ 2 giây.
         * Sau đó tạo listener mới.
         */
        restartDiscoveryDelayed(
                2000
        );
    }


    // =====================================================
    // CHECK API
    // =====================================================

    private boolean checkApi() {

        if (
                api == null
        ) {

            showConnectionSearching();


            tvMessage.setText(
                    "Chưa tìm thấy ESP32. App đang tự tìm..."
            );


            if (
                    !discoveryInProgress
            ) {

                restartDiscoveryDelayed(
                        300
                );
            }


            return false;
        }


        return true;
    }


    // =====================================================
    // REFRESH ALL
    // =====================================================

    private void refreshAll() {

        if (
                !checkApi()
        ) {

            return;
        }


        loadStatus();

        loadFingerprintCount();

        loadHistory();
    }


    // =====================================================
    // LOAD STATUS
    // =====================================================

    private void loadStatus() {

        if (
                !checkApi()
        ) {

            return;
        }


        final SmartDoorApi currentApi =
                api;


        currentApi.getStatus().enqueue(

                new Callback<StatusResponse>() {

                    @Override
                    public void onResponse(
                            Call<StatusResponse> call,
                            Response<StatusResponse> response
                    ) {

                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        if (
                                response.isSuccessful()
                                        &&
                                        response.body() != null
                        ) {

                            StatusResponse data =
                                    response.body();


                            showConnectionOnline();


                            String responseIp =
                                    data.getIp();


                            /*
                             * Neu app dang ket noi truc tiep SoftAP,
                             * giu nguyen 192.168.4.1.
                             *
                             * ESP32 co the tra ve IP cua WiFi ngoai trong JSON,
                             * nhung do KHONG phai duong app dang su dung.
                             */
                            boolean usingDirectAp =
                                    DIRECT_AP_IP.equals(
                                            currentEspIp
                                    );


                            if (
                                    !usingDirectAp
                                            &&
                                            responseIp != null
                                            &&
                                            !responseIp
                                                    .trim()
                                                    .isEmpty()
                            ) {

                                currentEspIp =
                                        responseIp;
                            }


                            String ipToDisplay =
                                    currentEspIp;


                            if (
                                    ipToDisplay == null
                                            ||
                                            ipToDisplay
                                                    .trim()
                                                    .isEmpty()
                            ) {

                                ipToDisplay =
                                        "--";
                            }


                            if (
                                    usingDirectAp
                            ) {

                                tvIp.setText(
                                        "IP ESP32: "
                                                +
                                                ipToDisplay
                                                +
                                                " (WiFi ESP32)"
                                );
                            }

                            else {

                                tvIp.setText(
                                        "IP ESP32: "
                                                +
                                                ipToDisplay
                                                +
                                                " (WiFi chung)"
                                );
                            }


                            if (
                                    data.isDoorLocked()
                            ) {

                                showDoorLocked();
                            }

                            else {

                                showDoorOpen();
                            }


                            showFingerprintStatus(
                                    data.isFingerprintConnected()
                            );


                            showRfidStatus(
                                    data.isRfidConnected()
                            );


                            if (
                                    usingDirectAp
                            ) {

                                tvMessage.setText(
                                        "Đã kết nối trực tiếp với SmartDoor_Direct."
                                );
                            }

                            else {

                                tvMessage.setText(
                                        "Smart Door đã kết nối qua WiFi chung."
                                );
                            }
                        }

                        else {

                            showConnectionOffline();


                            tvMessage.setText(
                                    "ESP32 được tìm thấy nhưng API /api/status không phản hồi."
                            );
                        }
                    }


                    @Override
                    public void onFailure(
                            Call<StatusResponse> call,
                            Throwable t
                    ) {

                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        handleConnectionLost(
                                "Mất kết nối ESP32. Đang chờ mạng ổn định để tìm lại..."
                        );
                    }
                }
        );
    }


    // =====================================================
    // OPEN DOOR
    // =====================================================

    private void openDoor() {

        if (
                !checkApi()
        ) {

            return;
        }


        tvMessage.setText(
                "Đang gửi lệnh mở cửa..."
        );


        btnOpenDoor.setEnabled(
                false
        );


        final SmartDoorApi currentApi =
                api;


        currentApi.openDoor().enqueue(

                new Callback<BasicResponse>() {

                    @Override
                    public void onResponse(
                            Call<BasicResponse> call,
                            Response<BasicResponse> response
                    ) {

                        btnOpenDoor.setEnabled(
                                true
                        );


                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        if (
                                response.isSuccessful()
                                        &&
                                        response.body() != null
                        ) {

                            String message =
                                    response.body()
                                            .getMessage();


                            if (
                                    message == null
                                            ||
                                            message
                                                    .trim()
                                                    .isEmpty()
                            ) {

                                message =
                                        "Đã mở cửa";
                            }


                            tvMessage.setText(
                                    message
                            );


                            loadStatus();

                            loadHistory();
                        }

                        else {

                            if (
                                    response.code() ==
                                            423
                            ) {

                                tvMessage.setText(
                                        "Hệ thống đang khóa tạm do xác thực sai quá 5 lần."
                                );
                            }

                            else if (
                                    response.code() ==
                                            409
                            ) {

                                tvMessage.setText(
                                        "Hệ thống đang xử lý thao tác khác. Vui lòng chờ."
                                );
                            }

                            else {

                                tvMessage.setText(
                                        getApiErrorMessage(
                                                response,
                                                "Mở cửa thất bại"
                                        )
                                );
                            }
                        }
                    }


                    @Override
                    public void onFailure(
                            Call<BasicResponse> call,
                            Throwable t
                    ) {

                        btnOpenDoor.setEnabled(
                                true
                        );


                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        handleConnectionLost(
                                "Không thể gửi lệnh mở cửa. Đang tìm lại ESP32..."
                        );
                    }
                }
        );
    }


    // =====================================================
    // LOCK DOOR
    // =====================================================

    private void lockDoor() {

        if (
                !checkApi()
        ) {

            return;
        }


        tvMessage.setText(
                "Đang gửi lệnh khóa cửa..."
        );


        btnLockDoor.setEnabled(
                false
        );


        final SmartDoorApi currentApi =
                api;


        currentApi.lockDoor().enqueue(

                new Callback<BasicResponse>() {

                    @Override
                    public void onResponse(
                            Call<BasicResponse> call,
                            Response<BasicResponse> response
                    ) {

                        btnLockDoor.setEnabled(
                                true
                        );


                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        if (
                                response.isSuccessful()
                                        &&
                                        response.body() != null
                        ) {

                            String message =
                                    response.body()
                                            .getMessage();


                            if (
                                    message == null
                                            ||
                                            message
                                                    .trim()
                                                    .isEmpty()
                            ) {

                                message =
                                        "Đã khóa cửa";
                            }


                            tvMessage.setText(
                                    message
                            );


                            loadStatus();
                        }

                        else {

                            tvMessage.setText(
                                    getApiErrorMessage(
                                            response,
                                            "Khóa cửa thất bại"
                                    )
                            );
                        }
                    }


                    @Override
                    public void onFailure(
                            Call<BasicResponse> call,
                            Throwable t
                    ) {

                        btnLockDoor.setEnabled(
                                true
                        );


                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        handleConnectionLost(
                                "Không thể gửi lệnh khóa cửa. Đang tìm lại ESP32..."
                        );
                    }
                }
        );
    }


    // =====================================================
    // GET FINGERPRINT ID
    // =====================================================

    private Integer getFingerprintId() {

        String text =
                edtFingerprintId
                        .getText()
                        .toString()
                        .trim();


        if (
                text.isEmpty()
        ) {

            Toast.makeText(
                    this,
                    "Vui lòng nhập ID vân tay",
                    Toast.LENGTH_SHORT
            ).show();


            return null;
        }


        try {

            int id =
                    Integer.parseInt(
                            text
                    );


            if (
                    id < 1
                            ||
                            id > 127
            ) {

                Toast.makeText(
                        this,
                        "ID phải từ 1 đến 127",
                        Toast.LENGTH_SHORT
                ).show();


                return null;
            }


            return id;
        }

        catch (
                NumberFormatException e
        ) {

            Toast.makeText(
                    this,
                    "ID không hợp lệ",
                    Toast.LENGTH_SHORT
            ).show();


            return null;
        }
    }


    // =====================================================
    // ENROLL FINGERPRINT
    // =====================================================

    private void enrollFingerprint() {

        if (
                !checkApi()
        ) {

            return;
        }


        Integer id =
                getFingerprintId();


        if (
                id == null
        ) {

            return;
        }


        tvMessage.setText(

                "Đang đăng ký ID "
                        +
                        id
                        +
                        ". Hãy làm theo hướng dẫn trên LCD."
        );


        btnEnrollFingerprint.setEnabled(
                false
        );


        final SmartDoorApi currentApi =
                api;


        currentApi.enrollFingerprint(
                id
        ).enqueue(

                new Callback<BasicResponse>() {

                    @Override
                    public void onResponse(
                            Call<BasicResponse> call,
                            Response<BasicResponse> response
                    ) {

                        btnEnrollFingerprint.setEnabled(
                                true
                        );


                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        if (
                                response.isSuccessful()
                                        &&
                                        response.body() != null
                        ) {

                            String message =
                                    response.body()
                                            .getMessage();


                            if (
                                    message == null
                                            ||
                                            message
                                                    .trim()
                                                    .isEmpty()
                            ) {

                                message =
                                        "Đăng ký vân tay thành công";
                            }


                            tvMessage.setText(
                                    message
                            );


                            edtFingerprintId.setText(
                                    ""
                            );


                            loadFingerprintCount();

                            loadStatus();
                        }

                        else {

                            if (
                                    response.code() ==
                                            423
                            ) {

                                tvMessage.setText(
                                        "Hệ thống đang khóa tạm."
                                );
                            }

                            else if (
                                    response.code() ==
                                            409
                            ) {

                                tvMessage.setText(
                                        getApiErrorMessage(
                                                response,
                                                "ID hoặc vân tay đã tồn tại."
                                        )
                                );
                            }

                            else if (
                                    response.code() ==
                                            422
                            ) {

                                tvMessage.setText(
                                        getApiErrorMessage(
                                                response,
                                                "Hai lần quét không giống nhau."
                                        )
                                );
                            }

                            else if (
                                    response.code() ==
                                            408
                            ) {

                                tvMessage.setText(
                                        getApiErrorMessage(
                                                response,
                                                "Quá thời gian đăng ký vân tay."
                                        )
                                );
                            }

                            else {

                                tvMessage.setText(
                                        getApiErrorMessage(
                                                response,
                                                "Đăng ký vân tay thất bại."
                                        )
                                );
                            }
                        }
                    }


                    @Override
                    public void onFailure(
                            Call<BasicResponse> call,
                            Throwable t
                    ) {

                        btnEnrollFingerprint.setEnabled(
                                true
                        );


                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        handleConnectionLost(
                                "Mất kết nối ESP32 trong khi đăng ký. Đang tìm lại..."
                        );
                    }
                }
        );
    }


    // =====================================================
    // DELETE FINGERPRINT
    // =====================================================

    private void deleteFingerprint() {

        if (
                !checkApi()
        ) {

            return;
        }


        Integer id =
                getFingerprintId();


        if (
                id == null
        ) {

            return;
        }


        tvMessage.setText(

                "Đang xóa vân tay ID "
                        +
                        id
        );


        btnDeleteFingerprint.setEnabled(
                false
        );


        final SmartDoorApi currentApi =
                api;


        currentApi.deleteFingerprint(
                id
        ).enqueue(

                new Callback<BasicResponse>() {

                    @Override
                    public void onResponse(
                            Call<BasicResponse> call,
                            Response<BasicResponse> response
                    ) {

                        btnDeleteFingerprint.setEnabled(
                                true
                        );


                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        if (
                                response.isSuccessful()
                                        &&
                                        response.body() != null
                        ) {

                            String message =
                                    response.body()
                                            .getMessage();


                            if (
                                    message == null
                                            ||
                                            message
                                                    .trim()
                                                    .isEmpty()
                            ) {

                                message =
                                        "Xóa vân tay thành công";
                            }


                            tvMessage.setText(
                                    message
                            );


                            edtFingerprintId.setText(
                                    ""
                            );


                            loadFingerprintCount();

                            loadStatus();
                        }

                        else {

                            if (
                                    response.code() ==
                                            423
                            ) {

                                tvMessage.setText(
                                        "Hệ thống đang khóa tạm."
                                );
                            }

                            else if (
                                    response.code() ==
                                            409
                            ) {

                                tvMessage.setText(
                                        "Hệ thống đang xử lý thao tác khác."
                                );
                            }

                            else if (
                                    response.code() ==
                                            404
                            ) {

                                tvMessage.setText(
                                        getApiErrorMessage(
                                                response,
                                                "ID vân tay không tồn tại."
                                        )
                                );
                            }

                            else {

                                tvMessage.setText(
                                        getApiErrorMessage(
                                                response,
                                                "Xóa vân tay thất bại."
                                        )
                                );
                            }
                        }
                    }


                    @Override
                    public void onFailure(
                            Call<BasicResponse> call,
                            Throwable t
                    ) {

                        btnDeleteFingerprint.setEnabled(
                                true
                        );


                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        handleConnectionLost(
                                "Mất kết nối ESP32 khi xóa vân tay. Đang tìm lại..."
                        );
                    }
                }
        );
    }


    // =====================================================
    // FINGERPRINT COUNT
    // =====================================================

    private void loadFingerprintCount() {

        if (
                !checkApi()
        ) {

            return;
        }


        final SmartDoorApi currentApi =
                api;


        currentApi.getFingerprintCount().enqueue(

                new Callback<CountResponse>() {

                    @Override
                    public void onResponse(
                            Call<CountResponse> call,
                            Response<CountResponse> response
                    ) {

                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        if (
                                response.isSuccessful()
                                        &&
                                        response.body() != null
                        ) {

                            tvFingerprintCount.setText(

                                    "Số mẫu vân tay đang lưu: "
                                            +
                                            response.body()
                                                    .getCount()
                            );
                        }

                        else {

                            tvFingerprintCount.setText(
                                    "Số mẫu vân tay đang lưu: --"
                            );
                        }
                    }


                    @Override
                    public void onFailure(
                            Call<CountResponse> call,
                            Throwable t
                    ) {

                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        tvFingerprintCount.setText(
                                "Số mẫu vân tay đang lưu: --"
                        );
                    }
                }
        );
    }


    // =====================================================
    // HISTORY
    // =====================================================

    private void loadHistory() {

        if (
                !checkApi()
        ) {

            return;
        }


        final SmartDoorApi currentApi =
                api;


        currentApi.getHistory().enqueue(

                new Callback<HistoryResponse>() {

                    @Override
                    public void onResponse(
                            Call<HistoryResponse> call,
                            Response<HistoryResponse> response
                    ) {

                        if (
                                api != currentApi
                        ) {

                            return;
                        }


                        if (
                                !response.isSuccessful()
                                        ||
                                        response.body() == null
                        ) {

                            return;
                        }


                        layoutHistory.removeAllViews();


                        List<HistoryItem> list =
                                response.body()
                                        .getHistory();


                        if (
                                list == null
                                        ||
                                        list.isEmpty()
                        ) {

                            addEmptyHistory();

                            return;
                        }


                        for (
                                HistoryItem item : list
                        ) {

                            addHistoryItem(
                                    item
                            );
                        }
                    }


                    @Override
                    public void onFailure(
                            Call<HistoryResponse> call,
                            Throwable t
                    ) {

                        // Giữ history cũ
                    }
                }
        );
    }


    // =====================================================
    // READ API ERROR
    // =====================================================

    private String getApiErrorMessage(
            Response<?> response,
            String defaultMessage
    ) {

        if (
                response == null
        ) {

            return defaultMessage;
        }


        try {

            ResponseBody errorBody =
                    response.errorBody();


            if (
                    errorBody == null
            ) {

                return defaultMessage;
            }


            String jsonText =
                    errorBody.string();


            if (
                    jsonText
                            .trim()
                            .isEmpty()
            ) {

                return defaultMessage;
            }


            JSONObject json =
                    new JSONObject(
                            jsonText
                    );


            String message =
                    json.optString(
                            "message",
                            ""
                    );


            if (
                    message == null
                            ||
                            message
                                    .trim()
                                    .isEmpty()
            ) {

                return defaultMessage;
            }


            return message;
        }

        catch (
                Exception e
        ) {

            return defaultMessage;
        }
    }

    // =====================================================
    // UI - CONNECTION
    // =====================================================

    private void showConnectionOnline() {

        tvConnection.setText(
                "●  ESP32 ONLINE"
        );

        tvConnection.setTextColor(
                Color.parseColor("#047857")
        );

        GradientDrawable background =
                new GradientDrawable();

        background.setColor(
                Color.parseColor("#D1FAE5")
        );

        background.setCornerRadius(
                dpToPx(100)
        );

        tvConnection.setBackground(
                background
        );

        btnEnrollFingerprint.setEnabled(
                true
        );

        btnDeleteFingerprint.setEnabled(
                true
        );

        btnRefresh.setEnabled(
                true
        );
    }


    private void showConnectionSearching() {

        tvConnection.setText(
                "●  ĐANG TÌM ESP32..."
        );

        tvConnection.setTextColor(
                Color.parseColor("#B45309")
        );

        GradientDrawable background =
                new GradientDrawable();

        background.setColor(
                Color.parseColor("#FEF3C7")
        );

        background.setCornerRadius(
                dpToPx(100)
        );

        tvConnection.setBackground(
                background
        );

        btnOpenDoor.setEnabled(
                false
        );

        btnLockDoor.setEnabled(
                false
        );

        btnEnrollFingerprint.setEnabled(
                false
        );

        btnDeleteFingerprint.setEnabled(
                false
        );
    }


    private void showConnectionOffline() {

        tvConnection.setText(
                "●  ESP32 OFFLINE"
        );

        tvConnection.setTextColor(
                Color.parseColor("#B91C1C")
        );

        GradientDrawable background =
                new GradientDrawable();

        background.setColor(
                Color.parseColor("#FEE2E2")
        );

        background.setCornerRadius(
                dpToPx(100)
        );

        tvConnection.setBackground(
                background
        );


        // Khi ESP32 offline, ứng dụng không thể biết trạng thái
        // hiện tại của AS608 và RC522 nên không giữ trạng thái OK cũ.
        tvFingerprintStatus.setText(
                "●  AS608 KHÔNG KẾT NỐI"
        );

        tvFingerprintStatus.setTextColor(
                Color.parseColor("#6B7280")
        );

        tvRfidStatus.setText(
                "●  RC522 KHÔNG KẾT NỐI"
        );

        tvRfidStatus.setTextColor(
                Color.parseColor("#6B7280")
        );

        tvFingerprintCount.setText(
                "Số mẫu vân tay đang lưu: --"
        );


        btnOpenDoor.setEnabled(
                false
        );

        btnLockDoor.setEnabled(
                false
        );

        btnEnrollFingerprint.setEnabled(
                false
        );

        btnDeleteFingerprint.setEnabled(
                false
        );
    }


    // =====================================================
    // UI - DOOR
    // =====================================================

    private void showDoorLocked() {

        if (
                tvDoorIcon != null
        ) {

            tvDoorIcon.setText(
                    "🔒"
            );
        }

        tvDoorStatus.setText(
                "CỬA ĐANG KHÓA"
        );

        GradientDrawable background =
                new GradientDrawable(
                        GradientDrawable.Orientation.TL_BR,
                        new int[] {
                                Color.parseColor("#2563EB"),
                                Color.parseColor("#1E3A8A")
                        }
                );

        background.setCornerRadius(
                dpToPx(24)
        );

        if (
                cardDoor != null
        ) {

            cardDoor.setBackground(
                    background
            );
        }

        btnOpenDoor.setEnabled(
                true
        );

        btnLockDoor.setEnabled(
                false
        );

        btnOpenDoor.setAlpha(
                1.0f
        );

        btnLockDoor.setAlpha(
                0.72f
        );
    }


    private void showDoorOpen() {

        if (
                tvDoorIcon != null
        ) {

            tvDoorIcon.setText(
                    "🔓"
            );
        }

        tvDoorStatus.setText(
                "CỬA ĐANG MỞ"
        );

        GradientDrawable background =
                new GradientDrawable(
                        GradientDrawable.Orientation.TL_BR,
                        new int[] {
                                Color.parseColor("#10B981"),
                                Color.parseColor("#047857")
                        }
                );

        background.setCornerRadius(
                dpToPx(24)
        );

        if (
                cardDoor != null
        ) {

            cardDoor.setBackground(
                    background
            );
        }

        btnOpenDoor.setEnabled(
                false
        );

        btnLockDoor.setEnabled(
                true
        );

        btnOpenDoor.setAlpha(
                0.72f
        );

        btnLockDoor.setAlpha(
                1.0f
        );
    }


    // =====================================================
    // UI - SENSOR STATUS
    // =====================================================

    private void showFingerprintStatus(
            boolean connected
    ) {

        if (
                connected
        ) {

            tvFingerprintStatus.setText(
                    "✓  AS608 OK"
            );

            tvFingerprintStatus.setTextColor(
                    Color.parseColor("#059669")
            );
        }

        else {

            tvFingerprintStatus.setText(
                    "✕  AS608 LỖI"
            );

            tvFingerprintStatus.setTextColor(
                    Color.parseColor("#DC2626")
            );
        }
    }


    private void showRfidStatus(
            boolean connected
    ) {

        if (
                connected
        ) {

            tvRfidStatus.setText(
                    "✓  RC522 OK"
            );

            tvRfidStatus.setTextColor(
                    Color.parseColor("#059669")
            );
        }

        else {

            tvRfidStatus.setText(
                    "✕  RC522 LỖI"
            );

            tvRfidStatus.setTextColor(
                    Color.parseColor("#DC2626")
            );
        }
    }


    // =====================================================
    // UI - HISTORY
    // =====================================================

    private void addEmptyHistory() {

        LinearLayout emptyCard =
                new LinearLayout(
                        this
                );

        emptyCard.setOrientation(
                LinearLayout.VERTICAL
        );

        emptyCard.setGravity(
                Gravity.CENTER
        );

        emptyCard.setPadding(
                dpToPx(20),
                dpToPx(20),
                dpToPx(20),
                dpToPx(20)
        );

        GradientDrawable background =
                new GradientDrawable();

        background.setColor(
                Color.WHITE
        );

        background.setCornerRadius(
                dpToPx(16)
        );

        background.setStroke(
                dpToPx(1),
                Color.parseColor("#E5E7EB")
        );

        emptyCard.setBackground(
                background
        );


        TextView text =
                new TextView(
                        this
                );

        text.setText(
                "Chưa có lịch sử mở cửa"
        );

        text.setTextColor(
                Color.parseColor("#6B7280")
        );

        text.setTextSize(
                14
        );

        text.setGravity(
                Gravity.CENTER
        );

        text.setPadding(
                0,
                dpToPx(6),
                0,
                0
        );

        emptyCard.addView(
                text
        );

        layoutHistory.addView(
                emptyCard
        );
    }


    private void addHistoryItem(
            HistoryItem item
    ) {

        LinearLayout card =
                new LinearLayout(
                        this
                );

        card.setOrientation(
                LinearLayout.HORIZONTAL
        );

        card.setGravity(
                Gravity.CENTER_VERTICAL
        );

        card.setPadding(
                dpToPx(14),
                dpToPx(14),
                dpToPx(14),
                dpToPx(14)
        );


        GradientDrawable cardBackground =
                new GradientDrawable();

        cardBackground.setColor(
                Color.WHITE
        );

        cardBackground.setCornerRadius(
                dpToPx(16)
        );

        cardBackground.setStroke(
                dpToPx(1),
                Color.parseColor("#E5E7EB")
        );

        card.setBackground(
                cardBackground
        );


        String method =
                item.getMethod() == null
                        ? ""
                        : item.getMethod();


        LinearLayout info =
                new LinearLayout(
                        this
                );

        info.setOrientation(
                LinearLayout.VERTICAL
        );

        LinearLayout.LayoutParams infoParams =
                new LinearLayout.LayoutParams(
                        0,
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        1
                );


        TextView methodText =
                new TextView(
                        this
                );

        String detail =
                item.getDetail() == null
                        ? ""
                        : item.getDetail();


        String methodDisplay;

        if (
                method.contains(
                        "XAC THUC SAI"
                )
        ) {

            if (
                    detail.contains(
                            "VAN TAY"
                    )
            ) {

                methodDisplay =
                        "Xác thực vân tay sai";
            }

            else if (
                    detail.contains(
                            "MAT KHAU"
                    )
            ) {

                methodDisplay =
                        "Xác thực mật khẩu sai";
            }

            else if (
                    detail.contains(
                            "RFID"
                    )
                            ||
                            detail.contains(
                                    "THE"
                            )
            ) {

                methodDisplay =
                        "Xác thực thẻ RFID sai";
            }

            else {

                methodDisplay =
                        "Xác thực sai";
            }
        }

        else {

            methodDisplay =
                    method.isEmpty()
                            ? "Mở cửa"
                            : getMethodDisplayName(
                            method
                    );
        }


        methodText.setText(
                methodDisplay
        );

        methodText.setTextColor(
                Color.parseColor("#111827")
        );

        methodText.setTextSize(
                15
        );

        methodText.setTypeface(
                null,
                Typeface.BOLD
        );


        TextView detailText =
                new TextView(
                        this
                );

        detailText.setText(
                detail
        );

        detailText.setTextColor(
                Color.parseColor("#6B7280")
        );

        detailText.setTextSize(
                13
        );


        String time =
                item.getTime() == null
                        ? "--"
                        : item.getTime();


        TextView timeText =
                new TextView(
                        this
                );

        timeText.setText(
                time
        );

        timeText.setTextColor(
                Color.parseColor("#9CA3AF")
        );

        timeText.setTextSize(
                12
        );

        timeText.setPadding(
                0,
                dpToPx(3),
                0,
                0
        );


        info.addView(
                methodText
        );

        if (
                !detail.isEmpty()
        ) {

            info.addView(
                    detailText
            );
        }

        info.addView(
                timeText
        );

        card.addView(
                info,
                infoParams
        );


        LinearLayout.LayoutParams cardParams =
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                );

        cardParams.setMargins(
                0,
                0,
                0,
                dpToPx(10)
        );


        layoutHistory.addView(
                card,
                cardParams
        );
    }


    private String getMethodDisplayName(
            String method
    ) {

        if (
                method.contains(
                        "VAN TAY"
                )
        ) {

            return "Mở bằng vân tay";
        }

        if (
                method.contains(
                        "RFID"
                )
        ) {

            return "Mở bằng thẻ RFID";
        }

        if (
                method.contains(
                        "MAT KHAU"
                )
        ) {

            return "Mở bằng mật khẩu";
        }

        if (
                method.contains(
                        "APP"
                )
        ) {

            return "Mở bằng ứng dụng";
        }

        return method;
    }


    private int dpToPx(
            int dp
    ) {

        float density =
                getResources()
                        .getDisplayMetrics()
                        .density;

        return Math.round(
                dp * density
        );
    }

}
