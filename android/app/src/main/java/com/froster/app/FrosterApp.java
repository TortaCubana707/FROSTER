package com.froster.app;

import android.app.Application;

import com.google.firebase.FirebaseApp;
import com.google.firebase.FirebaseOptions;
import com.google.firebase.database.FirebaseDatabase;
import com.froster.app.ui.Notificaciones;

/**
 * Arranque de la app. Firebase se configura aquí con BuildConfig (no hace falta
 * google-services.json) y se activa la caché en disco: sin internet, la app sigue
 * mostrando los últimos datos y guarda los comandos para enviarlos al volver la red.
 */
public class FrosterApp extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        FirebaseOptions opciones = new FirebaseOptions.Builder()
                .setApiKey(BuildConfig.FIREBASE_API_KEY)
                .setApplicationId(BuildConfig.FIREBASE_APP_ID)
                .setDatabaseUrl(BuildConfig.FIREBASE_DATABASE_URL)
                .setProjectId(BuildConfig.FIREBASE_PROJECT_ID)
                .build();
        if (FirebaseApp.getApps(this).isEmpty()) FirebaseApp.initializeApp(this, opciones);
        FirebaseDatabase.getInstance().setPersistenceEnabled(true);
        Notificaciones.crearCanal(this);
    }
}
