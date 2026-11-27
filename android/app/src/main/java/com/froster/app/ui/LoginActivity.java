package com.froster.app.ui;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.froster.app.R;
import com.google.firebase.auth.FirebaseAuth;

/** Inicio de sesión con la cuenta de Firebase. La sesión queda guardada en el teléfono. */
public class LoginActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (FirebaseAuth.getInstance().getCurrentUser() != null) { abrirTablero(); return; }
        setContentView(R.layout.activity_login);

        EditText correo = findViewById(R.id.txt_correo);
        EditText clave = findViewById(R.id.txt_clave);
        Button entrar = findViewById(R.id.btn_entrar);
        ProgressBar progreso = findViewById(R.id.progreso);

        entrar.setOnClickListener(v -> {
            String c = correo.getText().toString().trim();
            String k = clave.getText().toString();
            if (c.isEmpty() || k.isEmpty()) {
                Toast.makeText(this, R.string.falta_correo_clave, Toast.LENGTH_SHORT).show();
                return;
            }
            entrar.setEnabled(false);
            progreso.setVisibility(View.VISIBLE);
            FirebaseAuth.getInstance().signInWithEmailAndPassword(c, k)
                    .addOnSuccessListener(r -> abrirTablero())
                    .addOnFailureListener(e -> {
                        entrar.setEnabled(true);
                        progreso.setVisibility(View.GONE);
                        Toast.makeText(this, getString(R.string.no_se_pudo_entrar, e.getMessage()),
                                Toast.LENGTH_LONG).show();
                    });
        });
    }

    private void abrirTablero() {
        startActivity(new Intent(this, MainActivity.class));
        finish();
    }
}
