package com.aleongson.cloudcoffee;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.support.v7.app.NotificationCompat;
import android.util.Log;
import android.widget.Toast;

import com.aleongson.cloudcoffee.async.SendCoffeeRequestTask;
import com.aleongson.cloudcoffee.async.WaitCoffeeFinishedTask;

/**
 * 22/10/2015.
 */
public class XivelyTcpService extends Service implements
        SendCoffeeRequestTask.ISendCoffeeRequestListener, WaitCoffeeFinishedTask.IWaitCoffeeFinishedListener {
    private final IBinder binder = new LocalBinder();
    private Thread backgroundThread;
    private final static int REQUEST_CODE = 100;
    private final static int NOTIFICATION_ID = 103;
    private boolean hasNotification = false;

    private boolean hasRequestPending = false;
    private boolean hasResponsePending = false;
    private boolean isMinimized = false;

    private int requestResponseCode = -1;
    private long reqId = 0;

    public void setMinimized(boolean b) {
        isMinimized = b;
    }

    @Override
    public void sendCoffeeRequestPreExecute() {}

    @Override
    public void sendCoffeeRequestPostExecute(Integer result) {
        Log.v("SENDCOF", Integer.toString(result));
        hasResponsePending = true;
        requestResponseCode = result;
    }

    @Override
    public void waitCoffeeFinishedPreExecute() {}

    @Override
    public void waitCoffeeFinishedPostExecute(Integer result) {
        Log.v("SENDCOF", Integer.toString(result));
        hasResponsePending = true;
        requestResponseCode = result;

        if(isMinimized) {
            sendNotification();
        }
    }


    @Override
    public void onCreate() {
        super.onCreate();
        Log.v("HERE", "test");
        Toast.makeText(this, "Creating Service...", Toast.LENGTH_SHORT).show();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i("HERE", "Service started by startService()");
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.i("HERE", "onBind called...");
        return binder;
    }


    public class LocalBinder extends Binder {
        XivelyTcpService getService() {
            return XivelyTcpService.this;
        }
    }

    @Override
    public void onDestroy() {
        Log.i("TEST", "DESTROYING SERVICE");
        Toast.makeText(this, "Destroying Service...", Toast.LENGTH_SHORT).show();

        Thread dummy = backgroundThread;
        backgroundThread = null;
        //dummy.interrupt();
    }

    private void sendNotification() {
//        we use the compatibility library
        NotificationCompat.Builder builder = new NotificationCompat.Builder(this);
        builder.setSmallIcon(R.mipmap.ic_launcher)
                .setContentTitle("Cloud Coffee").setContentText("Your coffee is ready.")
                .setTicker("Your coffee is ready.")
                .setWhen(System.currentTimeMillis())
                .setOngoing(true);
        Intent startIntent = new Intent(this, ProcessingCoffeeActivity.class);
        PendingIntent contentIntent = PendingIntent.getActivity(this,
                REQUEST_CODE, startIntent, 0);
        builder.setContentIntent(contentIntent);
        Notification notification = builder.build();
        NotificationManager notificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.notify(NOTIFICATION_ID, notification);
        hasNotification = true;
    }

    public void cancelNotification() {
        if(hasNotification) {
            Log.v("HERE", "HERE");
            NotificationManager notificationManager =
                    (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            Log.i("HERE", "Cancelling notification");
            notificationManager.cancel(NOTIFICATION_ID);
            hasNotification = false;
        }
    }

    public boolean sendCoffeeRequest(String feedId, String apiKey, int coffee, int sugar, int creamer) {
        if(!hasRequestPending && !hasResponsePending) {
            reqId = System.currentTimeMillis() / 1000;
            new SendCoffeeRequestTask(this, feedId, apiKey, reqId).execute(coffee, sugar, creamer);
            hasRequestPending = true;
            return true;
        }
        return false;
    }

    public boolean waitCoffeeFinished(String feedId, String apiKey) {
        if(!hasRequestPending && !hasResponsePending) {
            new WaitCoffeeFinishedTask(this, feedId, apiKey, reqId).execute();
            hasRequestPending = true;
            return true;
        }
        return false;
    }

    public boolean hasResponse() {
        return hasResponsePending;
    }

    public int getResponseCode() {
        hasRequestPending = false;
        hasResponsePending = false;
        return requestResponseCode;
    }

    public long getReqId() {
        return reqId;
    }
}
