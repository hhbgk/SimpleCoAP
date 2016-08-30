package com.hhbgk.coap.api;

import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;

import com.hhbgk.coap.bean.CoAPRequest;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class CoAPClient {
    private final String tag = getClass().getSimpleName();
    static {
        System.loadLibrary("jl_coap_dtls");
    }

    public static final int COAP_REQUEST_GET       = 1;
    public static final int COAP_REQUEST_POST      = 2;

    public static final int COAP_NO_TOKEN = -1;
    private boolean isSecure = false;//switch of Datagram Transport Layer Security
    private String mServerIP;
    private List<OnResponseListener> mResponseListeners = new ArrayList<>();
    private SparseArray<OnResponseListener> mSparseArray = new SparseArray<> ();

    private static final int MSG_REQUEST = 1000;
    /** call from native code
     * @param data the data received from remote.
     */
    public synchronized void onDataReceived(short mid, short token, byte[] data) {
        Log.w(tag, "message id="+ mid+ ", token=" + token + ", onDataReceived:" + new String(data));
        OnResponseListener listener = mSparseArray.get(mid);
        if (listener != null){
            listener.onSuccess(data);
            mSparseArray.remove(mid);
        }
    }

    private HandlerThread mHandlerThread;
    private Handler mHandler;
    public CoAPClient(String serverIP){
        this(serverIP, false);
    }
    public CoAPClient(String serverIP, boolean isSecure){
        mServerIP = serverIP;
        this.isSecure = isSecure;
        nativeInit();
        _setup(serverIP, isSecure);

        mHandlerThread = new HandlerThread("HandlerThread_"+new Random().nextInt(Integer.MAX_VALUE));
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper(), new Handler.Callback() {
            @Override
            public boolean handleMessage(Message msg) {
                switch (msg.what) {
                    case MSG_REQUEST:
                        break;
                }
                return false;
            }
        });
    }
    private native void nativeInit();
    private native boolean _setup(String ip, boolean isSecure);
    private native short _request(int method, short token, String url, String[] query, String payload);
    //private native boolean _request(int method, String url);
    private native boolean _destroy();

    public interface OnResponseListener {
        void onSuccess(byte[] data);
        void onFailure(String message);
    }

    public void release(){
        mResponseListeners.clear();
        _destroy();
    }

    public void setSecure(boolean secure){
        isSecure = secure;
    }

    public void request(final CoAPRequest request, final OnResponseListener listener){
        if (listener == null)
            throw new NullPointerException("@param listener is null");
        if (TextUtils.isEmpty(mServerIP)){
            listener.onFailure("The param @resource is null.");
            return ;
        }
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                short msgId = _request(request.getMethod(), request.getToken(), request.getCommand(), request.getCmdParam(), request.getPayload());
                Log.e(tag, "msgId=" + msgId);
                if (msgId < 0){
                    listener.onFailure("Fail to send the request.");
                } else {
                    mSparseArray.put((int) msgId, listener);
                }
            }
        });

        /*Message message = Message.obtain();
        message.what = MSG_REQUEST;
        message.obj = listener;
        Bundle bundle = new Bundle();
        bundle.putInt("method", );*/
    }
}
