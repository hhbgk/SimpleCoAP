package com.hhbgk.coap;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import com.hhbgk.coap.api.CoAPClient;
import com.hhbgk.coap.bean.CoAPRequest;

public class MainActivity extends AppCompatActivity {
    private final String tag = getClass().getSimpleName();
    private CoAPClient coAPClient;
    private EditText mEditText;
    private EditText mEditPayload;
    private EditText mEditCmdParam;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mEditPayload = (EditText) findViewById(R.id.edit_payload);
        mEditCmdParam = (EditText) findViewById(R.id.edit_query);

        if (coAPClient == null) {
            coAPClient = new CoAPClient("192.168.9.141");
        }

        mEditText = (EditText) findViewById(R.id.edit_url);

        Button get = (Button) findViewById(R.id.get);
        assert get != null;
        get.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                CoAPRequest coAPRequest = new CoAPRequest();
                coAPRequest.setMethod(CoAPClient.COAP_REQUEST_GET);
                coAPRequest.setCommand(mEditText.getText().toString().trim());
                coAPRequest.setToken((short) 5);

                coAPRequest.setPayload(mEditPayload.getText().toString().trim());
                request(coAPRequest);
            }
        });

        Button postBtn = (Button) findViewById(R.id.post);
        assert postBtn != null;
        postBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                CoAPRequest coAPRequest = new CoAPRequest();
                coAPRequest.setMethod(CoAPClient.COAP_REQUEST_POST);
                coAPRequest.setCommand(mEditText.getText().toString().trim());
                coAPRequest.setToken((short) 5);

                coAPRequest.setPayload(mEditPayload.getText().toString().trim());
                request(coAPRequest);
            }
        });
    }

    private void request(CoAPRequest request){

        coAPClient.request(request, new CoAPClient.OnResponseListener() {
            @Override
            public void onSuccess(byte[] data) {
                Log.w(tag, "receive:" + new String(data));
            }

            @Override
            public void onFailure(String message) {
                Log.e(tag, "fail to setup CoAP");
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (coAPClient != null) {
            coAPClient.release();
        }
    }
}
