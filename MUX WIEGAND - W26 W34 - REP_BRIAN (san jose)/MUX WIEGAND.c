//2021/06/17 Se invierte etiqueta de lectora para corregir problema del sistema con nips (soo puertos a y b)
//2021/06/16 Se emula codigo de mux de Brian
//2020/09/30 Actualizado Ahora se pueden conectar tarjetas wiegand26 o 34 sin reprogramar
//           Ahora tambien envian la tecla pulsada
//           Compatible con lectoras Zkteco y Dahua
//           Caso Mifare: Las lectoras Dahua envian el paquete en un oreden distinto NO MEZCLAR con otra marca
//           Caso ID: se pueden mezclar las marcas
//2017/11/15
#include <16F727.h>

#include <stdlib.h>
#use delay (clock=20000000)
#fuses HS,PROTECT,NOWDT//WDT_2304MS
#use rs232(uart1, baud=115200,TIMEOUT=10)

////SALIDAS
#define indicadorA PIN_D2//
#define indicadorB PIN_D3//
#define indicadorC PIN_D0//
#define indicadorD PIN_D1//
#define salida01 PIN_C0//
#define salida02 PIN_C1//
#define salida03 PIN_C2//
#define salida04 PIN_C5//

//AD1-B0,AD0-B1,BD1-B2,BD0-B3,CD0-B4
int16 i;
////////VARIABLES WIEGAND/////////
const int wieg_size= 34;//tamaño maximo 
////
int cta_reini;
int apagarA,time_apagarA,apagarB,time_apagarB,apagarC,time_apagarC,apagarD,time_apagarD;

int deteccion_nulo_a,pre_cuenta_a;
int nipA[4],nipB[4],nipC[4],nipD[4],time_nip[4],f_nip[4];
int ia,ib,ic,id,mi,relay[4],t_relay[4];
int wieg_a;
int sub_indice_a=0;
int wieg_full_a=0;
int wiegand_cuenta_a;
unsigned int32 deci_a;
char data_a[wieg_size];
////
int deteccion_nulo_b,pre_cuenta_b;
int wieg_b;
int sub_indice_b=0;
int wieg_full_b=0;
int wiegand_cuenta_b;
unsigned int32 deci_b;
char data_b[wieg_size];
////
int deteccion_nulo_c,pre_cuenta_c;
int wieg_c;
int sub_indice_c=0;
int wieg_full_c=0;
int wiegand_cuenta_c;
unsigned int32 deci_c;
char data_c[wieg_size];
////
int deteccion_nulo_d,pre_cuenta_d;
int wieg_d;
int sub_indice_d=0;
int wieg_full_d=0;
int wiegand_cuenta_d;
unsigned int32 deci_d;
char data_d[wieg_size];
///////////////////////////////
void wiegand_read_card();

#int_TIMER1 //se utiliza timer 1 porque el 0 esta asignado al wdt
void TIMER1_isr(void){
//Nuestro código de interrupción.
wiegand_cuenta_a++;
wiegand_cuenta_b++;
wiegand_cuenta_c++;
wiegand_cuenta_d++;
time_apagarA++;
time_apagarB++;
time_apagarC++;
time_apagarD++;
for(mi=0;mi<4;mi++) {
   time_nip[mi]++;
   t_relay[mi]++;}
set_timer1(0x3CB0);//80ms
}

#INT_RB
void INT_RB_isr(void){
///////////LECTORA A
 if (!input(PIN_B0) ){//data1
   output_high(indicadorA);
   while (!input(PIN_B0) ) {}
   wiegand_cuenta_a=0;
   wieg_a=1;
   data_a[sub_indice_a]=1;
   sub_indice_a++;
   if(sub_indice_a==wieg_size)  wieg_full_a=1;
 }
 if (!input(PIN_B1) ){//data0
   output_high(indicadorA);
   while (!input(PIN_B1) ) {}
   wiegand_cuenta_a=0;
   wieg_a=1;
   data_a[sub_indice_a]=0;
   sub_indice_a++;
   if(sub_indice_a==wieg_size)  wieg_full_a=1;
 }
///////////LECTORA B
 if (!input(PIN_B2) ){//data1
   output_high(indicadorB);
   while (!input(PIN_B2) ) {}
   wiegand_cuenta_b=0;
   wieg_b=1;
   data_b[sub_indice_b]=1;
   sub_indice_b++;
   if(sub_indice_b==wieg_size)  wieg_full_b=1;
 }//
 if (!input(PIN_B3) ){//data0
   output_high(indicadorB);
   while (!input(PIN_B3) ) {}
   wiegand_cuenta_b=0;
   wieg_b=1;
   data_b[sub_indice_b]=0;
   sub_indice_b++;
   if(sub_indice_b==wieg_size)  wieg_full_b=1;
 }
///////////LECTORA C
 if (!input(PIN_B4) ){//data1
   output_high(indicadorC);
   while (!input(PIN_B4) ) {}
   wiegand_cuenta_c=0;
   wieg_c=1;
   data_c[sub_indice_c]=1;
   sub_indice_c++;
   if(sub_indice_c==wieg_size)  wieg_full_c=1;
 }//
 if (!input(PIN_B5) ){//data0
   output_high(indicadorC);
   while (!input(PIN_B5) ) {}
   wiegand_cuenta_c=0;
   wieg_c=1;
   data_c[sub_indice_c]=0;
   sub_indice_c++;
   if(sub_indice_c==wieg_size)  wieg_full_c=1;
 }
///////////LECTORA D
 if (!input(PIN_B6) ){//data1
   output_high(indicadorD);
   while (!input(PIN_B6) ) {}
   wiegand_cuenta_d=0;
   wieg_d=1;
   data_d[sub_indice_d]=1;
   sub_indice_d++;
   if(sub_indice_d==wieg_size)  wieg_full_d=1;
 }//
 if (!input(PIN_B7) ){//data0
   output_high(indicadorD);
   while (!input(PIN_B7) ) {}
   wiegand_cuenta_d=0;
   wieg_d=1;
   data_d[sub_indice_d]=0;
   sub_indice_d++;
   if(sub_indice_d==wieg_size)  wieg_full_d=1;
 }

}

void main() {
//setup_oscillator(OSC_8MHZ|OSC_INTRC);//<<<<<<<<<<<<<<<
setup_adc_ports(NO_ANALOGS);
setup_adc(ADC_OFF);
setup_timer_1(T1_INTERNAL|T1_DIV_BY_8);//Setup timer: Reloj interno, preescaler= 8
enable_interrupts(INT_TIMER1);//Habilito interrupción particular del TIMER1
set_timer1(0x3CB0);//Carga del TMR1

ext_int_edge(INT_RB,H_TO_L);       //Asigno flancos de subida
enable_interrupts(INT_RB);//interrupcion de puerto
enable_interrupts(INT_RB0);
enable_interrupts(INT_RB1);
enable_interrupts(INT_RB2);
enable_interrupts(INT_RB3);
enable_interrupts(INT_RB4);
enable_interrupts(INT_RB5);
enable_interrupts(INT_RB6);
enable_interrupts(INT_RB7);
cta_reini++;
//printf("\r\nINICIADO:%u",cta_reini);
/////////////////
wieg_full_a=0;
deci_a=0;
deteccion_nulo_a=0;
pre_cuenta_a=0;
wieg_a=sub_indice_a=i=0;
wiegand_cuenta_a=2;
for(i=0;i<wieg_size;i++) data_a[i]=0;
//
wieg_full_b=0;
deci_b=0;
deteccion_nulo_b=0;
pre_cuenta_b=0;
wieg_b=sub_indice_b=i=0;
wiegand_cuenta_b=2;
for(i=0;i<wieg_size;i++) data_b[i]=0;
wieg_full_c=0;
deci_c=0;
deteccion_nulo_c=0;
pre_cuenta_c=0;
wieg_c=sub_indice_c=i=0;
wiegand_cuenta_c=2;
for(i=0;i<wieg_size;i++) data_c[i]=0;
//
wieg_full_d=0;
deci_d=0;
deteccion_nulo_d=0;
pre_cuenta_d=0;
wieg_d=sub_indice_d=i=0;
wiegand_cuenta_d=2;
for(i=0;i<wieg_size;i++) data_d[i]=0;

enable_interrupts(GLOBAL);//Habilito interrupciones globales
apagarA=apagarB=apagarC=apagarD=0;

memset(nipA, 0, sizeof(nipA));
memset(nipB, 0, sizeof(nipB));
memset(nipC, 0, sizeof(nipC));
memset(nipD, 0, sizeof(nipD));
memset(time_nip, 0, sizeof(time_nip));
memset(f_nip, 0, sizeof(f_nip));
ia=ib=ic=id=0;
memset(relay, 0, sizeof(relay));
memset(t_relay, 0, sizeof(t_relay));
output_low(salida01);
output_low(salida02);
output_low(salida03);
output_low(salida04);
while(true){
//////////////////
 if( (apagarA)&&(time_apagarA>=25) ) {
   apagarA=0;
   output_low(indicadorA);
 }
 if( (apagarB)&&(time_apagarB>=25) ) {
   apagarB=0;
   output_low(indicadorB);
 }
 if( (apagarC)&&(time_apagarC>=25) ) {
   apagarC=0;
   output_low(indicadorC);
 }
 if( (apagarD)&&(time_apagarD>=25) ) {
   apagarD=0;
   output_low(indicadorD);
 }
 wiegand_read_card();
 for(i=0;i<4;i++) {//apaga luz roja (nip cancelado)
   if( relay[i]&&(t_relay[i]>=6) ){
      relay[i]=0;
      switch(i){
         case 0: output_low(salida01);
                 break;
         case 1: output_low(salida02);
                 break;
         case 2: output_low(salida03);
                 break;
         case 3: output_low(salida04);
                 break;
      }
   }//
 }//end for

 for(i=0;i<4;i++) {//12.5=1seg.
   if( f_nip[i]&&(time_nip[i])>50){
      relay[i]=1;
      t_relay[i]=0;
      f_nip[i]=0;
      switch(i){
         case 0: memset(nipA, 0, sizeof(nipA));
                 ia=0;
                 output_high(salida01);
                 break;
         case 1: memset(nipB, 0, sizeof(nipB));
                 ib=0;
                 output_high(salida02);
                 break;
         case 2: memset(nipC, 0, sizeof(nipC));
                 ic=0;
                 output_high(salida03);
                 break;
         case 3: memset(nipD, 0, sizeof(nipD));
                 id=0;
                 output_high(salida04);
                 break;
      }//
   }//end tiempo
 }//end 4seg.
 restart_wdt();
 }//fin while true
}//fin main

void wiegand_read_card(){
 if( (wieg_a==1)&&(wiegand_cuenta_a>=2) ) {
   disable_interrupts(GLOBAL);//Deshabilito las interrupciones globales
//!   printf("\r\nTipo:W%u ",sub_indice_a);
   wieg_full_a=0;
   deci_a=0;
   
   if( (sub_indice_a==26)||(sub_indice_a==34) ){
      for(i=1;i<sub_indice_a-1;i++) deci_a = (deci_a<<1)|data_a[i];
      printf("MUXA-IDX:%08LX\r",deci_a);
      apagarA=1;
      time_apagarA=0;
      memset(nipA, 0, sizeof(nipA));
      ia=0;
      f_nip[0]=0;
   }
   else if ((sub_indice_a==8)||(sub_indice_a==4) ) { //printf("Tecla:");
         apagarA=0;
         output_low(indicadorA);
         if(sub_indice_a==4) for(i=0;i<sub_indice_a;i++) deci_a = (deci_a<<1)|data_a[i];
         else for(i=4;i<sub_indice_a;i++) deci_a = (deci_a<<1)|data_a[i];
         nipA[ia]=deci_a;
         f_nip[0]=1;
         time_nip[0]=0;
         ia++;
         if(ia==4) {
            printf("MUXB-NIP:%u%u%u%u\r",nipA[0],nipA[1],nipA[2],nipA[3]);//MUXA-IDX:00C4DA5D
            memset(nipA, 0, sizeof(nipA));
            ia=0;
            f_nip[0]=0;
         }
   }
   else {
//!         printf("Error de Lectura:");
         disable_interrupts(GLOBAL);//Deshabilito las interrupciones globales
         apagarA=0;
         output_low(indicadorA);
//!         for(i=0;i<sub_indice_a;i++) printf("%u",data_a[i]);
   }
   deci_a=0;
   deteccion_nulo_a=0;
   pre_cuenta_a=0;
   wieg_a=sub_indice_a=i=0;
   //for(i=0;i<wieg_size;i++) data_a[i]=0;
   memset(data_a, 0, sizeof(data_a) );
   enable_interrupts(GLOBAL);
 }//END TARJETA LEIDA
 //////
 if( (wieg_b==1)&&(wiegand_cuenta_b>=2) ) {
   disable_interrupts(GLOBAL);//Deshabilito las interrupciones globales
//!   printf("\r\nTipo:W%u ",sub_indice_b);
   wieg_full_b=0;
   deci_b=0;
   
   if( (sub_indice_b==26)||(sub_indice_b==34) ){
      for(i=1;i<sub_indice_b-1;i++) deci_b = (deci_b<<1)|data_b[i];
      printf("MUXB-IDX:%08LX\r",deci_b);
      apagarB=1;
      time_apagarB=0;
      memset(nipB, 0, sizeof(nipB));
      ib=0;
   }
   else if ( (sub_indice_b==8)||(sub_indice_b==4) ) {
//!         printf("Tecla:");
         apagarB=0;
         output_low(indicadorB);
         if(sub_indice_b==8) for(i=4;i<sub_indice_b;i++) deci_b = (deci_b<<1)|data_b[i];
         else for(i=0;i<sub_indice_b;i++) deci_b = (deci_b<<1)|data_b[i];
         nipB[ib]=deci_b;
         f_nip[1]=1;
         time_nip[1]=0;
         ib++;
         if(ib==4) {
            printf("MUXA-NIP:%u%u%u%u\r",nipB[0],nipB[1],nipB[2],nipB[3]);//MUXA-IDX:00C4DA5D
            memset(nipB, 0, sizeof(nipB));
            ib=0;
            f_nip[1]=0;
         }
//!         printf("%Lu-B\r",deci_b);
   }
   else {
//!         printf("Error de Lectura:");
         disable_interrupts(GLOBAL);//Deshabilito las interrupciones globales
         apagarB=0;
         output_low(indicadorB);
         memset(nipB, 0, sizeof(nipB));
         ib=0;
//!         for(i=0;i<sub_indice_b;i++) printf("%u",data_b[i]);
   }
   deci_b=0;
   deteccion_nulo_b=0;
   pre_cuenta_b=0;
   wieg_b=sub_indice_b=i=0;
   //for(i=0;i<wieg_size;i++) data_a[i]=0;
   memset(data_b, 0, sizeof(data_b) );
   enable_interrupts(GLOBAL);
 }//END TARJETA LEIDA
 //////
 if( (wieg_c==1)&&(wiegand_cuenta_c>=2) ) {
   disable_interrupts(GLOBAL);//Deshabilito las interrupciones globales
//!   printf("\r\nTipo:W%u ",sub_indice_c);
   wieg_full_c=0;
   deci_c=0;
   
   if( (sub_indice_c==26)||(sub_indice_c==34) ){
      for(i=1;i<sub_indice_c-1;i++) deci_c = (deci_c<<1)|data_c[i];
      printf("MUXC-IDX:%08LX\r",deci_c);
      apagarC=1;
      time_apagarC=0;
      f_nip[2]=0;
   }
   else if ( (sub_indice_c==8)||(sub_indice_c==4) ) {//!         printf("Tecla:");
         apagarC=0;
         output_low(indicadorC);
         if(sub_indice_c==8) for(i=4;i<sub_indice_c;i++) deci_c = (deci_c<<1)|data_c[i];
         else for(i=0;i<sub_indice_c;i++) deci_c = (deci_c<<1)|data_c[i];
         nipC[ic]=deci_C;
         f_nip[2]=1;
         time_nip[2]=0;
         ic++;
         if(ic==4) {
            printf("MUXC-NIP:%u%u%u%u\r",nipC[0],nipC[1],nipC[2],nipC[3]);//MUXA-IDX:00C4DA5D
            memset(nipC, 0, sizeof(nipC));
            ic=0;
            f_nip[2]=0;
         }
//!         printf("%Lu-C\r",deci_c);
   }
   else {
//!         printf("Error de Lectura:");
         disable_interrupts(GLOBAL);//Deshabilito las interrupciones globales
         apagarC=0;
         output_low(indicadorC);
//!         for(i=0;i<sub_indice_c;i++) printf("%u",data_c[i]);
   }
   deci_c=0;
   deteccion_nulo_c=0;
   pre_cuenta_c=0;
   wieg_c=sub_indice_c=i=0;
   memset(data_c, 0, sizeof(data_c) );
   enable_interrupts(GLOBAL);
 }//END TARJETA LEIDA
 //////
 if( (wieg_d==1)&&(wiegand_cuenta_d>=2) ) {
   disable_interrupts(GLOBAL);//Deshabilito las interrupciones globales
//!   printf("\r\nTipo:W%u ",sub_indice_d);
   wieg_full_d=0;
   deci_d=0;
   
   if( (sub_indice_d==26)||(sub_indice_d==34) ){
      for(i=1;i<sub_indice_d-1;i++) deci_d = (deci_d<<1)|data_d[i];
      printf("MUXD-IDX:%08LX\r",deci_d);
      apagarD=1;
      time_apagarD=0;
      f_nip[3]=0;
   }
   else if( (sub_indice_d==8)||(sub_indice_d==4) ) {//!         printf("Tecla:");
         apagarD=0;
         output_low(indicadorD);
         if(sub_indice_d==8) for(i=4;i<sub_indice_d;i++) deci_d = (deci_d<<1)|data_d[i];
         else for(i=0;i<sub_indice_d;i++) deci_d = (deci_d<<1)|data_d[i];
         nipD[id]=deci_d;
         f_nip[3]=1;
         time_nip[3]=0;
         id++;
         if(id==4) {
            printf("MUXD-NIP:%u%u%u%u\r",nipD[0],nipD[1],nipD[2],nipD[3]);//MUXA-IDX:00C4DA5D
            memset(nipD, 0, sizeof(nipD));
            id=0;
            f_nip[1]=0;
         }
//!         printf("%Lu-D\r",deci_d);
   }
   else {
//!         printf("Error de Lectura:");
         disable_interrupts(GLOBAL);//Deshabilito las interrupciones globales
         apagarD=0;
         output_low(indicadorD);
//!         for(i=0;i<sub_indice_d;i++) printf("%u",data_d[i]);
   }
   deci_d=0;
   deteccion_nulo_d=0;
   pre_cuenta_d=0;
   wieg_d=sub_indice_d=i=0;
   memset(data_d, 0, sizeof(data_d) );
   enable_interrupts(GLOBAL);
 }//END TARJETA LEIDA
 //////
} 


