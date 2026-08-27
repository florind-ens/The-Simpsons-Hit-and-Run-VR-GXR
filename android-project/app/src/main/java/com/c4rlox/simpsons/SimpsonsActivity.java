package com.simpsonsHitAndRun.vr;

import android.Manifest;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;

import org.libsdl.app.SDLActivity;

public class SimpsonsActivity extends SDLActivity {
    private static final int STORAGE_PERMISSION_REQUEST = 1001;
    private boolean storagePermissionRequested;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestGameStorageAccess();
    }

    @Override
    protected void onDestroy() {
        // Let SDL deliver its quit event and release OpenXR/EGL first.
        super.onDestroy();

        // SHAR and its legacy middleware own process-lifetime native static
        // state and cannot safely execute SDL_main twice in one process.
        // Quest commonly retains an empty Activity process after exit, so
        // terminate it once the final Activity teardown is complete. The next
        // launcher press then always starts from a clean native process.
        if (isFinishing() && !isChangingConfigurations()) {
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }

    private void requestGameStorageAccess() {
        if (storagePermissionRequested) {
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (Environment.isExternalStorageManager()) {
                return;
            }

            storagePermissionRequested = true;
            try {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + getPackageName()));
                startActivity(intent);
            } catch (ActivityNotFoundException ignored) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            storagePermissionRequested = true;
            requestPermissions(new String[] {
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE
            }, STORAGE_PERMISSION_REQUEST);
        }
    }
}
