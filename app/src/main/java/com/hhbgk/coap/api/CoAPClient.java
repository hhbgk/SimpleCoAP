package com.hhbgk.coap.api;

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
    private boolean isSecure = false;//switch of Datagram Transport Layer Security
    private String mServerIP;
    private List<OnResponseListener> mResponseListeners = new ArrayList<>();
    private SparseArray<OnResponseListener> mSparseArray = new SparseArray<> ();
    /** call from native code
     * @param data the data received from remote.
     */
    public synchronized void onDataReceived(int mid, short token, byte[] data) {
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
        this(serverIP, true);
    }
    public CoAPClient(String serverIP, boolean isSecure){
        mServerIP = serverIP;
        nativeInit();
        _setup(serverIP, isSecure);

        mHandlerThread = new HandlerThread("HandlerThread_"+new Random().nextInt(Integer.MAX_VALUE));
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper(), new Handler.Callback() {
            @Override
            public boolean handleMessage(Message msg) {
                switch (msg.what) {
                    case 0:
                        Log.w(tag, "___request____________");
                        boolean result = _request((Long) msg.obj);
                        Log.w(tag, "___request____________OK");
                        if (!result){
                            Log.e(tag, "Fail to send the request.");
                        }
                        break;
                }
                return false;
            }
        });
    }
    private native void nativeInit();
    private native boolean _setup(String ip, boolean isSecure);
    private native long[] _new_request(int method, short token, String url, String[] query, String payload);
    //private native boolean _request(int method, String url);
    private native boolean _request(long requestAddress);
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

    public void request(CoAPRequest request, OnResponseListener listener){
        if (listener == null)
            throw new NullPointerException("@param listener is null");
        if (TextUtils.isEmpty(mServerIP)){
            listener.onFailure("The param @resource is null.");
            return ;
        }
        //mResponseListeners.add(listener);
        String prefix;
        if (isSecure) {
            prefix = "coaps://";
        } else {
            prefix = "coap://";
        }
        //String url = prefix + mServerIP+ "/" + command;
        long[] address = _new_request(request.getMethod(), request.getToken(), request.getCommand(), request.getCmdParam(), request.getPayload());
        Log.e(tag, "address[0]=" + address[0]);
        mSparseArray.put((int) address[1], listener);
//        mHandler.removeMessages(0);
//        mHandler.sendMessage(mHandler.obtainMessage(0, address[0]));

        boolean result = _request(address[0]);
        address[0] = 0;
//        boolean result = _request(method, prefix + mServerIP+ "/" + resource);
        if (!result){
            listener.onFailure("Fail to send the request.");
        }
    }
}
