package com.hhbgk.coap.bean;

/**
 * Author: bob
 * Date: 16-8-17 09:03
 * Version: V1
 * Description:
 */
public class CoAPRequest {
    private int method;//CoAP request method
    private String command;//CoAP URI
    private String[] cmdParam;//Command param
    private String payload;//CoAP payload
    private short token;//CoAP token

    public int getMethod() {
        return method;
    }

    public void setMethod(int method) {
        this.method = method;
    }

    public String getCommand() {
        return command;
    }

    public void setCommand(String command) {
        this.command = command;
    }

    public String[] getCmdParam() {
        return cmdParam;
    }

    public void setCmdParam(String[] cmdParam) {
        this.cmdParam = cmdParam;
    }

    public String getPayload() {
        return payload;
    }

    public void setPayload(String payload) {
        this.payload = payload;
    }

    public short getToken() {
        return token;
    }

    public void setToken(short token) {
        this.token = token;
    }
}
