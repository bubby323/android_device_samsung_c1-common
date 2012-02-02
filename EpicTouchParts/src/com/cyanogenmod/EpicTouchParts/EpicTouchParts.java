package com.cyanogenmod.EpicTouchParts;

import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.PreferenceActivity;

public class EpicTouchParts extends PreferenceActivity  {

    public static final String MDNIE_UI = "mdnie_ui";
    public static final String MDNIE_USER = "mdnie_user";
    public static final String KEY_BACKLIGHT_TIMEOUT = "backlight_timeout";
    public static final String NOTIFICATION_TIMEOUT = "notification_timeout";
    public static final String NOTIFICATION_ENABLED = "notification_enabled";

    private ListPreference mmDNIeUIMode;
    private ListPreference mmDNIeUserMode;
    private ListPreference mBacklightTimeout;
    private ListPreference mNotificationTimeout;
    private ListPreference mNotificationEnabled;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.main);

        mmDNIeUIMode = (ListPreference) findPreference(MDNIE_UI);
        mmDNIeUIMode.setEnabled(NotificationEnabled.isSupported());
        mmDNIeUIMode.setOnPreferenceChangeListener(new mDNIeUIMode());

        mmDNIeUserMode = (ListPreference) findPreference(MDNIE_USER);
        mmDNIeUserMode.setEnabled(NotificationEnabled.isSupported());
        mmDNIeUserMode.setOnPreferenceChangeListener(new mDNIeUserMode());

        mNotificationEnabled = (ListPreference) findPreference(NOTIFICATION_ENABLED);
        mNotificationEnabled.setEnabled(NotificationEnabled.isSupported());
        mNotificationEnabled.setOnPreferenceChangeListener(new NotificationEnabled());

        mNotificationTimeout = (ListPreference) findPreference(NOTIFICATION_TIMEOUT);
        mNotificationTimeout.setEnabled(NotificationTimeout.isSupported());
        mNotificationTimeout.setOnPreferenceChangeListener(new NotificationTimeout());

        mBacklightTimeout = (ListPreference) findPreference(KEY_BACKLIGHT_TIMEOUT);
        mBacklightTimeout.setEnabled(TouchKeyBacklightTimeout.isSupported());
        mBacklightTimeout.setOnPreferenceChangeListener(new TouchKeyBacklightTimeout());

    }

}