#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "logica_extraida.h"
// modelo identico al de probar_logica.cpp, version minima
struct P { double cam=26,tubo=25,agua=8,hoja=8,cuarto=25,carga=0,hielo=900000;
  void paso(double dt,bool A,bool B,bool bomba){
    double n=(A?1:0)+(B?1:0),qhot=0,qc=0;
    for(int i=0;i<(int)n;i++){double c=22.0-0.5*(hoja-tubo); if(c<0)c=0; qc+=c; qhot+=c+12;}
    double qcond=(n==0)?0.4*(hoja-tubo):0, G=bomba?4.0:1.0, q=G*(cam-tubo);
    cam+=dt*(carga-q-2.5*(cam-cuarto))/CC; tubo+=dt*(q-qc+qcond)/150;
    double qn=qhot+(bomba?6:0)-0.5*(agua-cuarto)-qcond;
    if(hielo>0&&qn>0){hielo-=qn*dt;agua=8;if(hielo<0){agua+=-hielo/12500;hielo=0;}} else agua+=dt*qn/12500;
    hoja=agua+0.12*qhot; }
  double CC=700; };
int main(int argc,char**argv){
  double cc = argc>1 ? atof(argv[1]) : 700;
  bool sinBanda = argc>2 && atoi(argv[2]);
  printf("capacidad %.0f J/K, banda adaptativa %s\n  carga | cambios/h | ciclos rapidos (max) | camara min..max (tras 15 min)\n", cc, sinBanda ? "DESACTIVADA" : "activa");
  for(double carga=14; carga<=44; carga+=4){
    P p; p.carga=carga; p.CC=cc; Control c={}; Entradas e={NAN,NAN,NAN,28.0f,MODO_AUTO,false,false,false};
    bool A=false,B=false,bm=false; int camb=0; uint8_t maxc=0; double mn=1e9,mx=-1e9;
    for(uint32_t t=2000;t<=3600000;t+=2000){
      p.paso(2,A,B,bm); e.tCamara=p.cam; e.tTubo=p.tubo; e.tAgua=p.hoja;
      if (sinBanda) c.ciclosRapidos = 0;
      Salidas s=decidir(e,c,t);
      camb += (s.celdaA!=A)+(s.celdaB!=B); A=s.celdaA; B=s.celdaB; bm=s.bomba;
      if(c.ciclosRapidos>maxc) maxc=c.ciclosRapidos;
      if(t>900000){ if(p.cam<mn)mn=p.cam; if(p.cam>mx)mx=p.cam; } }
    printf("  %4.0f W | %9d | %20d | %.1f .. %.1f\n", carga, camb, maxc, mn, mx);
  }
}
