package com.froster.app.datos;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.NonNull;

import com.google.firebase.database.ChildEventListener;
import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.DatabaseError;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;
import com.google.firebase.database.Query;
import com.google.firebase.database.ValueEventListener;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.function.Supplier;

/**
 * Todo el acceso a datos de la app: Firebase en tiempo real y Room en el teléfono.
 * Las consultas de Room corren en un hilo aparte y los resultados regresan al hilo
 * principal, así la interfaz nunca se congela (sin ANR) aunque no haya internet.
 */
public class Repositorio {

    public interface AlCambiar { void con(DataSnapshot datos); }
    public interface ResultadoComando {
        void confirmado(long milisegundos);
        void fallo(String motivo);
    }
    public interface Resultado<T> { void con(T valor); }

    private static final long ESPERA_ACK_MS = 5000;
    private static final long GUARDAR_LECTURA_CADA_MS = 30000;   // historial sin saturar el teléfono

    private static Repositorio instancia;

    private final FirebaseDatabase firebase = FirebaseDatabase.getInstance();
    private final BaseDatos base;
    private final ExecutorService hilo = Executors.newSingleThreadExecutor();
    private final Handler principal = new Handler(Looper.getMainLooper());
    private final Map<Long, Pendiente> pendientes = new HashMap<>();
    private long ultimaLecturaGuardada = 0;
    private int nivelActual = 0, alertaActual = 0;
    private Double objetivoActual;

    private static class Pendiente {
        final long inicio = SystemClock.elapsedRealtime();
        final ResultadoComando resultado;
        Pendiente(ResultadoComando resultado) { this.resultado = resultado; }
    }

    public static synchronized Repositorio de(Context contexto) {
        if (instancia == null) instancia = new Repositorio(contexto);
        return instancia;
    }

    private Repositorio(Context contexto) {
        base = BaseDatos.de(contexto);
        firebase.getReference(Contrato.ACK).addValueEventListener(oyente(this::alLlegarAck));
        firebase.getReference(Contrato.EVENTOS).limitToLast(50).addChildEventListener(guardarEventos);
    }

    // ------------------------------------------------------------ tiempo real

    /**
     * Una escucha activa sobre Firebase. La pantalla la cancela en onStop() para no gastar
     * datos ni batería mientras no se ve.
     */
    public static final class Suscripcion {
        private final Query consulta;
        private final ValueEventListener oyente;

        private Suscripcion(Query consulta, ValueEventListener oyente) {
            this.consulta = consulta;
            this.oyente = oyente;
        }

        public void cancelar() { consulta.removeEventListener(oyente); }
    }

    /** Escucha una ruta completa (lecturas, estado, config, dispositivo o .info/connected). */
    public Suscripcion escuchar(String ruta, AlCambiar alCambiar) {
        return suscribir(firebase.getReference(ruta), alCambiar);
    }

    /** Escucha solo los últimos eventos que agrega el ESP32 (no descarga la lista completa). */
    public Suscripcion escucharUltimosEventos(int limite, AlCambiar alCambiar) {
        return suscribir(firebase.getReference(Contrato.EVENTOS).orderByKey().limitToLast(limite), alCambiar);
    }

    private static Suscripcion suscribir(Query consulta, AlCambiar alCambiar) {
        ValueEventListener o = oyente(alCambiar);
        consulta.addValueEventListener(o);
        return new Suscripcion(consulta, o);
    }

    private static ValueEventListener oyente(AlCambiar alCambiar) {
        return new ValueEventListener() {
            @Override public void onDataChange(@NonNull DataSnapshot s) { alCambiar.con(s); }
            @Override public void onCancelled(@NonNull DatabaseError e) { /* sin permiso o sin sesión */ }
        };
    }

    // ------------------------------------------------------------ comandos

    /**
     * Manda un comando a config/ con cmd_id y espera la confirmación del ESP32 en ack/.
     * Ver docs/CONTRATO_FIREBASE.md.
     */
    public void enviarComando(Map<String, Object> cambios, ResultadoComando resultado) {
        long id = System.currentTimeMillis();
        Map<String, Object> datos = new HashMap<>(cambios);
        datos.put("cmd_id", id);
        pendientes.put(id, new Pendiente(resultado));
        principal.postDelayed(() -> {
            Pendiente p = pendientes.remove(id);
            if (p != null) p.resultado.fallo("El ESP32 no confirmó el comando");
        }, ESPERA_ACK_MS);
        DatabaseReference config = firebase.getReference(Contrato.CONFIG);
        config.updateChildren(datos).addOnFailureListener(e -> {
            Pendiente p = pendientes.remove(id);
            if (p != null) p.resultado.fallo("No se pudo enviar: " + e.getMessage());
        });
    }

    private void alLlegarAck(DataSnapshot ack) {
        Long id = ack.child("cmd_id").getValue(Long.class);
        Pendiente p = id == null ? null : pendientes.remove(id);
        if (p != null) p.resultado.confirmado(SystemClock.elapsedRealtime() - p.inicio);
    }

    // ------------------------------------------------------------ historial (Room)

    /** Guarda una lectura cada 30 s como máximo. Lo llama la pantalla principal. */
    public void guardarLectura(DataSnapshot lecturas) {
        long ahora = System.currentTimeMillis();
        if (ahora - ultimaLecturaGuardada < GUARDAR_LECTURA_CADA_MS) return;
        ultimaLecturaGuardada = ahora;
        Lectura l = new Lectura();
        Long ts = lecturas.child("ts").getValue(Long.class);
        l.ts = ts != null ? ts : ahora;
        l.ambiente = lecturas.child("ambiente").getValue(Double.class);
        l.humedad = lecturas.child("humedad").getValue(Double.class);
        l.placaFria = lecturas.child("placa_fria").getValue(Double.class);
        l.fugaPct = lecturas.child("fuga_pct").getValue(Double.class);
        l.nivel = nivelActual;
        l.alertaId = alertaActual;
        l.objetivo = objetivoActual;
        l.excede = l.ambiente != null && objetivoActual != null && l.ambiente > objetivoActual;
        hilo.execute(() -> base.lecturas().insertar(l));
    }

    public void recordarEstado(int nivel, int alertaId, Double objetivo) {
        nivelActual = nivel;
        alertaActual = alertaId;
        if (objetivo != null) objetivoActual = objetivo;
    }

    private final ChildEventListener guardarEventos = new ChildEventListener() {
        @Override public void onChildAdded(@NonNull DataSnapshot s, String anterior) {
            Evento e = new Evento();
            e.clave = s.getKey() == null ? "" : s.getKey();
            e.tipo = s.child("tipo").getValue(String.class);
            e.texto = s.child("texto").getValue(String.class);
            Integer nivel = s.child("nivel").getValue(Integer.class);
            e.nivel = nivel == null ? 0 : nivel;
            e.camara = s.child("camara").getValue(Double.class);
            e.setpoint = s.child("setpoint").getValue(Double.class);
            Long ts = s.child("ts").getValue(Long.class);
            e.ts = ts == null ? System.currentTimeMillis() : ts;
            hilo.execute(() -> base.eventos().insertar(e));
        }
        @Override public void onChildChanged(@NonNull DataSnapshot s, String a) { }
        @Override public void onChildRemoved(@NonNull DataSnapshot s) { }
        @Override public void onChildMoved(@NonNull DataSnapshot s, String a) { }
        @Override public void onCancelled(@NonNull DatabaseError e) { }
    };

    /** Corre una consulta de Room en el hilo aparte y entrega el resultado en el hilo principal. */
    private <T> void consultar(Supplier<T> consulta, Resultado<T> resultado) {
        hilo.execute(() -> {
            T valor = consulta.get();
            principal.post(() -> resultado.con(valor));
        });
    }

    public void ultimasLecturas(int limite, Resultado<List<Lectura>> resultado) {
        consultar(() -> base.lecturas().ultimas(limite), resultado);
    }

    public void ultimosEventos(int limite, Resultado<List<Evento>> resultado) {
        consultar(() -> base.eventos().ultimos(limite), resultado);
    }

    /** Picos de temperatura excedidos guardados en el teléfono (criterio del Proyecto 1). */
    public void picos(Resultado<List<Evento>> resultado) {
        consultar(() -> base.eventos().picos(), resultado);
    }

    public void lecturasExcedidas(int limite, Resultado<List<Lectura>> resultado) {
        consultar(() -> base.lecturas().excedidas(limite), resultado);
    }
}
