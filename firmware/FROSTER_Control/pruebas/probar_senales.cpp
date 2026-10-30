// Prueba del LED RGB y del buzzer de FROSTER_Control en la PC (con funciones de Arduino simuladas).
// La corre probar.sh.
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include "logica_extraida.h"
#define PIN_LED_R 32
#define PIN_LED_G 33
#define PIN_LED_B 13
#define PIN_BUZZER 23
#define LED_ANODO_COMUN 0
#ifndef BUZZER_ACTIVO_BAJO
#define BUZZER_ACTIVO_BAJO 0
#endif
struct Nota { uint16_t hz, ms; };
static uint32_t T = 0; uint32_t millis() { return T; }
static int R=0,G=0,B=0; static std::vector<std::pair<uint32_t,int>> tonos;
static int ultimoBuzzer = -1;   // 0 silencio en bajo, 1 silencio en alto, 2 sonando
void ledcWrite(uint8_t p, uint32_t d){ if(p==32)R=d; if(p==33)G=d; if(p==13)B=d; if(p==23 && d==1023) ultimoBuzzer=1; }
uint32_t ledcWriteTone(uint8_t, uint32_t f){ tonos.push_back({T,(int)f}); ultimoBuzzer = f ? 2 : 0; return f; }
bool ledcAttach(uint8_t, uint32_t, uint8_t){ return true; }
#include "senales_extraidas.h"
int fallas=0;
#define OK(c,...) do{ if(!(c)){fallas++; printf("  [FALLA] " __VA_ARGS__); printf("\n");} }while(0)
Salidas S(uint8_t n, uint8_t al=A_NINGUNA, uint8_t mot=M_REPOSO){ Salidas s={}; s.nivel=n; s.alerta=al; s.motivo=mot; return s; }
void correr(const Salidas &s, uint32_t hasta){ while(T<hasta){ gestionarSenales(s); T+=10; } }
int sonidos(uint32_t a, uint32_t b){ int n=0; for(auto&x:tonos) if(x.first>=a&&x.first<b&&x.second>0) n++; return n; }
int main(){
  iniciarSenales();
  correr(S(0), 500);    OK(B==255 && R==0, "arrancando no es azul (encendido)");
  correr(S(0), 1000);   OK(B==0, "arrancando no parpadea (apagado)");
  OK(sonidos(0,1000)==0, "sono antes de 1.5 s");
  correr(S(1), 3000);   OK(sonidos(1500,3000)==3, "melodia de arranque: %d notas", sonidos(1500,3000));
  OK(ultimoBuzzer != 2, "el buzzer quedo sonando");
  OK(ultimoBuzzer == (BUZZER_ACTIVO_BAJO ? 1 : 0), "silencio con el nivel equivocado (%d)", ultimoBuzzer);
  correr(S(1), 11000);  OK(G==255&&R==0&&B==0, "nivel 1 no es verde (%d,%d,%d)",R,G,B);
  OK(sonidos(3000,11000)==0, "sono sin cambio de nivel");
  correr(S(2), 13000);  OK(G==200&&B==200, "nivel 2 no es cian"); OK(sonidos(11000,13000)==2, "subida a 2: %d", sonidos(11000,13000));
  correr(S(3), 15000);  OK(B==255&&G==0, "nivel 3 no es azul");   OK(sonidos(13000,15000)==3, "subida a 3: %d", sonidos(13000,15000));
  correr(S(1), 17000);  OK(sonidos(15000,17000)==2, "bajada: %d", sonidos(15000,17000));
  correr(S(1,A_CONGELA,M_CONGELA), 20000); OK(sonidos(17000,20000)==3, "congelamiento: %d", sonidos(17000,20000));
  correr(S(1,A_CONGELA,M_CONGELA), 70000); OK(sonidos(20000,70000)==0, "repitio antes del minuto");
  correr(S(1,A_CONGELA,M_CONGELA), 80000); OK(sonidos(70000,80000)==3, "no repitio al minuto");
  T=80200; gestionarSenales(S(1,A_CONGELA,M_CONGELA)); int r1=R; T=80600; gestionarSenales(S(1,A_CONGELA,M_CONGELA)); int r2=R;
  OK(r1!=r2, "la alerta no parpadea");
  correr(S(1,A_SENSOR,M_SENSOR), 83000); OK(sonidos(80600,83000)==3, "cambio de alerta no sono de inmediato");
  correr(S(2,A_NINGUNA,M_PRECIRC), 84000); OK(R==255&&G==180&&B==0, "pre-circulando no es amarillo");
  printf("senales (buzzer activo %s): %d fallas\n", BUZZER_ACTIVO_BAJO ? "bajo" : "alto", fallas); return fallas!=0;
}
