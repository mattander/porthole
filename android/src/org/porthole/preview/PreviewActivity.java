package org.porthole.preview;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.webkit.ConsoleMessage;
import android.webkit.CookieManager;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

/** Offline, removable preview. No permissions, services, bridge, or Home takeover. */
public final class PreviewActivity extends Activity {
    private static final String TAG = "PortholePreview";
    private static final String ASSETS = "file:///android_asset/";
    private static final String START = ASSETS + "index.html?demo=1";
    // IMMERSIVE_STICKY is an API 19 flag; the tested frame runs API 23.
    private static final int IMMERSIVE_STICKY = 0x1000;
    private WebView web;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        web = new WebView(this);
        WebSettings settings = web.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(false);
        settings.setDatabaseEnabled(false);
        settings.setSaveFormData(false);
        settings.setGeolocationEnabled(false);
        settings.setAllowFileAccess(false); // Packaged android_asset files remain accessible.
        settings.setAllowContentAccess(false);
        settings.setAllowFileAccessFromFileURLs(false);
        settings.setAllowUniversalAccessFromFileURLs(false);
        settings.setBlockNetworkLoads(true);
        settings.setCacheMode(WebSettings.LOAD_NO_CACHE);
        settings.setUseWideViewPort(true);
        settings.setLoadWithOverviewMode(true);
        settings.setTextZoom(100);
        CookieManager.getInstance().setAcceptCookie(false);
        web.setWebChromeClient(new WebChromeClient() {
            @Override public boolean onConsoleMessage(ConsoleMessage message) {
                if (message.message().startsWith("PORTHOLE_METRICS ")) {
                    Log.i(TAG, message.message());
                } else if (message.messageLevel() == ConsoleMessage.MessageLevel.ERROR) {
                    Log.e(TAG, "JavaScript: " + message.message() + " line " + message.lineNumber());
                }
                return true;
            }
        });
        web.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView view, String url) {
                return !url.startsWith(ASSETS);
            }
            @Override public void onPageFinished(WebView view, String url) {
                // Only layout/sample facts are logged; no browser-to-native JavaScript bridge.
                view.loadUrl("javascript:(function(){var d=document,b=d.body;"
                    + "function t(id){var e=d.getElementById(id);return e?e.textContent:'';}"
                    + "console.log('PORTHOLE_METRICS '+JSON.stringify({"
                    + "width:innerWidth,height:innerHeight,scrollWidth:b.scrollWidth,"
                    + "scrollHeight:b.scrollHeight,month:t('month-title'),notice:t('notice-text'),"
                    + "clock:t('clock'),dayButtons:d.querySelectorAll('.day-button').length}));})()");
            }
            @Override public void onReceivedError(WebView view, int code, String description, String url) {
                Log.e(TAG, "Preview load failed: " + code + " " + description);
            }
        });
        web.setOnLongClickListener(new View.OnLongClickListener() {
            @Override public boolean onLongClick(View view) {
                new AlertDialog.Builder(PreviewActivity.this).setTitle("Porthole preview")
                    .setItems(new String[] {"Reload sample", "Exit preview"},
                        new DialogInterface.OnClickListener() {
                            @Override public void onClick(DialogInterface dialog, int item) {
                                if (item == 0) web.loadUrl(START); else finish();
                            }
                        })
                    .setNegativeButton("Cancel", null).show();
                return true;
            }
        });
        setContentView(web);
        hideSystemBars();
        web.loadUrl(START);
    }

    private void hideSystemBars() {
        web.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | IMMERSIVE_STICKY);
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused && web != null) hideSystemBars();
    }
    @Override protected void onDestroy() {
        if (web != null) { web.stopLoading(); web.destroy(); web = null; }
        super.onDestroy();
    }
}
