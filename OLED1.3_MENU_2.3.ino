/*
* 26-07-2026  : 22:20 -> 00:40
* versione 2.4
* Versione precedente: 2.3
* Aggiornamenti:
**  Aggiunta Light Sleep mode
**  nuova funzione che gestisce il volume
**  migliorata gestione case Bro
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_HTU21DF.h>   // sensore umidità e temperatura
#include "SD_MMC.h"    // Libreria per usare la SD tramite bus SDMMC
#include "Audio.h"     // Libreria ESP32-audioI2S per riprodurre audio
#include <FluxGarage_RoboEyes.h>
#include <WiFi.h>

//OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

//Bottoni
#define BTU 32  //Bottone che va su
#define BTD 33  //Bottone che va giù
#define BTC 25  //Bottone di conferma
#define BTB 26  //Bottone back

//audio
#define I2S_BLCK 18     // BCK
#define I2S_LRC 19      // LCK
#define I2S_DOUT 21     // DIN

Audio audio;
Adafruit_SH1106G display(SCREEN_WIDTH ,SCREEN_HEIGHT ,&Wire, -1);
Adafruit_HTU21DF htu = Adafruit_HTU21DF();    // sensore di temperatura e umidità
RoboEyes roboEyes(display);


unsigned long tempo=0;        // generale
unsigned long tempoFace=0;    // Robo
unsigned long tempoTemp=0;    // temperatura
unsigned long tempoCaricamento=0;    // barra di caricamento
unsigned long tempoScansioneWifi = 0;
const char* lista[] = {
  "Bro",
  "AURA",
  "Canzoni",
  "Info",
  "WiFi",
  "Termini",
  "Condizioni",  
  "Exit",
  "SONAR",
  "PIR",
};

enum Menu {
  BRO,
  AURA,
  CANZONI,
  INFO,
  WIFI,
  TERMINI,
  CONDIZIONI,
  EXIT,
  SONAR,
  PIR,
};

int moods[]={DEFAULT, HAPPY, TIRED, ANGRY};

char nomiCanzoni[20][21];   // memoria vera dove sono i nomi   20 stringhe da 21 caratteri massimo l'una
const char* canzoni[20];    // PUNTATORI a nomiCanzoni, 20 stringhe lunghe 21 caratteri (incluso il \0).
// va bene const, i valori puntati potranno essere solo letti e non modificati
uint8_t numeroCanzoni=0;

char listaWiFi[25][19];     // 25 righe, lunghe 22 caratteri l'una (compreso il \0)
const char* nomiWifi[25];   // PUNTATORI A listaWifi
uint8_t bssid_sel [6];    // il bssid sono 6 byte di caratteri da 0 a 255, quindi conviene uint8_t

uint8_t len_lista = sizeof(lista) / sizeof(lista[0]);

int i=0;        // indice cursore
int j=0;        // indice menù
uint8_t curs_x = 7; // posizione x cursore
uint8_t curs_y = 19; // posizione y cursore
uint8_t curs_y_prec=curs_y;
uint8_t schermata=0;
int numWiFi=0;

uint8_t Pos_x=15;
uint8_t Pos_y=16;
uint8_t LargCaricamento=1;    // dimensione pixel barra di caricamento

uint8_t scelta = 0;


bool statoPrecC = true;
bool statoPrecB = true;

int volume = 3;
bool st_canzoni=false;
bool st_aura=false;
bool st_bro = false;
bool st_info = false;
bool st_wifi = false;
bool st_sch3 = false;

bool bssidValido = false;
bool scanningWiFi = false;

bool statoAttD=true;
bool statoAttU=true;
bool statoAttC=true;
bool statoAttB=true;


float temp = 0.0; // temperatura in gradi celsius
float umid = 0.0; // umidità

void gest_vol(){
  if (statoAttU == false && millis()-tempo>200){
          tempo=millis();
          volume+=1;
          if (volume>21)
            volume=21;
            st_volume();
            display.display();
      } else if (statoAttD == false && millis()-tempo>200){
            tempo=millis();
            volume-=1;
            if (volume<0)
              volume=0;
            st_volume();
            display.display();
          }
}

void st_volume(){
  display.fillRect(48,18,70,25,SH110X_BLACK);
  display.setCursor(48,19);
  display.print(volume);
  audio.setVolume(volume);
}

void titolo(const char* titolo){
  display.setCursor(0,0);
  display.println(titolo);
  display.drawLine(0, 10, 127, 10, SH110X_WHITE); // linea orizzontale
}

void stampa_seriale(){
  Serial.print(F("Hai selezionato: "));
  Serial.println(lista[i]);
  Serial.print(F("curs_x: "));
  Serial.print(curs_x);
  Serial.print(F("  | curs_y: "));
  Serial.println(curs_y);
}


void stampa_menu(int longlist, int &k, const char* list[]){
  int stampati=0;
  Pos_x=15;
  Pos_y=16;

  while (k<longlist && stampati<5){
    display.setCursor(Pos_x,Pos_y);
    display.println(list[k]);
    Pos_y+=9;
    k+=1;
    stampati+=1;
  }
  Serial.print(F("k= "));
  Serial.println(k);
}

bool isAudio(String nome){   // NON ho usato il &, avrei così da passare la stringa senza andare a crearne una copia, però modificando la string passata quindi se il file aveva maiuscole non verrà riconosciuto successivamente
  nome.toLowerCase();
  return nome.endsWith(".mp3") || nome.endsWith(".wav");   // ritorna true se finisce con .wav
}

void leggiCanzone(){
  File root = SD_MMC.open("/");
  File file = root.openNextFile();
  while (file){
    if (!file.isDirectory()){
      String nome = file.name();
      if (isAudio(nome) && numeroCanzoni<20){    // verifica che il file sia un adio.wav e che l'array abbia spazio
        snprintf(nomiCanzoni[numeroCanzoni],sizeof(nomiCanzoni[numeroCanzoni]),"%s",nome.c_str());    // se invece usassi sizeof(nomiCanzoni), siccome sono 20 righe da 21 byte: 20*21 = 420 byte andrei ad allocare a riga
        canzoni[numeroCanzoni] = nomiCanzoni[numeroCanzoni];      // funziona perché canzoni è un array di puntatori (numeri) qunidi posso usare =     
        numeroCanzoni++;
      }
    }
    file.close();
    file = root.openNextFile();                   // file diventa = al nome del prossimo file
  }
  root.close();
}

void gestBTU(int len, int &v, const char* list[] ){

        if (i == 0) { 
          statoPrecC = statoAttC;
          statoPrecB = statoAttB;
          return; 
        }
        i-=1;
              //Serial.println(i);
        if ((i+1)%5==0){    // se mi trovo alla prima riga o ultima riga.... gestisco cambio pagina
          display.fillRect(0,16,128,64, SH110X_BLACK);       // reset display
          curs_x = 7; // posizione x cursore
          curs_y = 55; // posizione y cursore
          display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE);     // disegno cursore
          v=(i+1)-5;
          stampa_menu(len,v,list);

        }else{    // altrimenti se non mi trovo alla prima o ultima riga
          display.fillRect(curs_x-5,curs_y-5,10,10, SH110X_BLACK);   // cancella cursore precedente
          curs_y-=9;                // sposto puntatore alla riga succesiva
          display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE); // cerchio pieno

                //Serial.print("il cursore si trova sulla riga: ");
                //Serial.println(lista[i]);
          }

          display.display();

}

void gestBTD(int len, int &v, const char* list []){
      //Serial.println("BT DOWN premuto!");

      if (i==len-1){
        return;
      }
      i+=1;
      //v=(i/5) * 5;
      if (i>0 && i%5==0){
        display.fillRect(0,16,128,64, SH110X_BLACK);
        curs_x = 7; // posizione x cursore
        curs_y = 19; // posizione y cursore
        display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE);
        stampa_menu(len,v,list);
        
      } else{
      display.fillRect(curs_x-5,curs_y-5,10,10, SH110X_BLACK);
      curs_y+=9;
      display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE); // cerchio pieno
      }
      //stampa_seriale();
      display.display();
}

void setup() {
  //init BT
  pinMode(BTD, INPUT_PULLUP);
  pinMode(BTU, INPUT_PULLUP);
  pinMode(BTC, INPUT_PULLUP);
  pinMode(BTB, INPUT_PULLUP);

  Serial.begin(115200);

  
  Wire.begin(22,23);
  // INIT sensore temperatura / umidità
  if (!htu.begin()){
    //while(true){
      Serial.println(F("Errore inizializzazione sensore temperatura/luminosità"));
      //delay(3000);
    //}
  }

  //INIT OLED
  if (!display.begin(0x3C,true)){
    while(true){
      Serial.println(F("Errore inizializzazione Display! Riavviare esp32"));
      delay(3000);
    }
  }

    // Inizializzo RobotEyes
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);   //tenta di aggiornare il volto 100 volte al secondo

  roboEyes.close();                // Occhi inizialmente chiusi
  roboEyes.setMood(DEFAULT);              // volto felice
  roboEyes.setPosition(DEFAULT);   // Posizione standard

  roboEyes.setAutoblinker(ON, 3, 2); // Blink automatico
  roboEyes.setIdleMode(ON, 3, 1);    // Movimenti idle

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);

  display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE); // cerchio pieno
  titolo("Menu'");
  // Stampa 
  stampa_menu(len_lista,j,lista);
  //j=(i/5) * 5;

  //Speaker
  audio.setPinout(I2S_BLCK,I2S_LRC,I2S_DOUT);
  audio.setVolume(volume);

  if(!SD_MMC.begin("/sdcard",true)){     // se non riesce ad inizializzare la microsd allora...
    Serial.println(F("Errore inizializzazione microsd"));
    display.fillCircle(126,2,1,SH110X_WHITE);
  } else{
    leggiCanzone();
  }

  //INIT WIFI
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  display.display();
}

void loop() {
  statoAttD=digitalRead(BTD);
  statoAttU=digitalRead(BTU);
  statoAttC=digitalRead(BTC);
  statoAttB=digitalRead(BTB);


  

  if (schermata == 0){

    if (statoAttD == false && millis()-tempo>200){
      tempo=millis();
      gestBTD(len_lista,j,lista);

    } else if (statoAttU == false && millis()-tempo>200){
      tempo=millis();
      gestBTU(len_lista,j,lista);

    }else if (statoAttC == false && statoPrecC == true && millis()-tempo>300){
      tempo=millis();
      Serial.print(F("Hai selezionato: "));
      Serial.println(lista[i]);
      display.fillRect(0,0,128,64,SH110X_BLACK);
      titolo(lista[i]);
      display.display();
      curs_y_prec = curs_y;
      Serial.println(curs_y_prec);
      curs_x = 7; // posizione x cursore
      curs_y = 19; // posizione y cursore
      j=0;
      scelta = i;
      i=0;
      schermata = 1;

      if (millis() - tempoScansioneWifi > 10000 && scelta==WIFI) {
          scanningWiFi = false;
          WiFi.scanDelete();
      }

    }
  } else if (schermata == 1){
    
      switch (scelta){
        case AURA:
            if (st_aura==false){
              //path = "/" + canzoni[1];
              audio.setVolume(volume);
              audio.connecttoFS(SD_MMC, "/aura.mp3");
              display.setCursor(5,19);
              display.print("Volume:");
              display.setCursor(48,19);
              display.print(volume);
              st_aura=true;
              display.display();

            }
            gest_vol();
            break;

        case CANZONI:

          if(st_canzoni == false){
            st_canzoni=true;
            Serial.println(F("Entrato in st_canzoni"));
            stampa_menu(numeroCanzoni,j,canzoni);
            Serial.println(F("Canzoni trovate:"));
              for (int c=0; c<numeroCanzoni;c++){
                Serial.println(canzoni[c]);
              }
            display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE); // cerchio pieno
            display.display();
          }
          if (statoAttU == false && millis()-tempo>200){
              tempo=millis();

              gestBTU(numeroCanzoni,j,canzoni);

            } else if (statoAttD == false && millis()-tempo>200){
                tempo=millis();

                gestBTD(numeroCanzoni,j,canzoni);

            } else if (statoAttC == false && statoPrecC == true && millis()-tempo >300){
                tempo=millis();
                display.fillRect(0,0,128,64,SH110X_BLACK);   // cancella schermata
                char path[41] = "/";
                snprintf(path,sizeof(path),"%s",canzoni[i]);    //snprintf(stringa iniziale, spazio massimo, formato, variabile char);
                //Serial.println(path);
                audio.setVolume(volume);
                audio.connecttoFS(SD_MMC, path);
                display.setCursor(5,19);
                display.print("Volume:");
                display.setCursor(48,19);
                display.print(volume);
                schermata = 2;
                st_volume();
                titolo(canzoni[i]);
                
                display.display();

            }
        break;

        case BRO:
          if (st_bro == false) {
            tempoFace = millis();
            st_bro = true;

            roboEyes.setIdleMode(ON, 2, 1);// Cambia direzione ogni 2-3 secondi
            // Lampeggia ogni 2-3 secondi
            roboEyes.setAutoblinker(ON, 2, 1);
          }

          if (millis() - tempoFace > random(4000,8000)) {// Cambia espressione ogni 4-8 secondi
            tempoFace = millis();

            int umoreCasuale = random(4);
            roboEyes.setMood(moods[umoreCasuale]);
          }

          roboEyes.update();
        break;

        case INFO:
          if (st_info==false){
            Serial.println(F("Entroato in st_info == false"));
            st_info=true;
            display.fillRect(0,16,128,64, SH110X_BLACK);
            display.setCursor(5,20);
            display.println("Temperatura:");
            display.setCursor(110,20);
            display.println("C");
            display.setCursor(5,32);
            display.println("Umidita':");
            display.setCursor(100,32);
            display.println("%");
            display.display();
          }
          if (millis()-tempoTemp>2000){
            tempoTemp = millis();
            temp = htu.readTemperature();
            umid = htu.readHumidity();

              Serial.print(F("Temperatura: "));
              Serial.print(temp);
              Serial.println(F(" C"));

              Serial.print(F("Umidita: "));
              Serial.print(umid);
              Serial.println(F(" %"));
            
            if (!isnan(temp)){
              display.fillRect(70,19,40,10, SH110X_BLACK);  // temperatura
              display.setCursor(75,20);
              display.println(temp);
            }
            if (!isnan(umid)){
              display.fillRect(63,31,35,10, SH110X_BLACK);  // umidità
              display.setCursor(64,32);
              display.println(umid);
            }
            display.display();
          }
        break;


        case WIFI:

          if (st_wifi == false){
            
            if (scanningWiFi == false){
              WiFi.scanNetworks(true);    // Avvia scansione WiFi asincrona
              display.setCursor(0,20);
              display.println("Scansione in corso");
              display.drawRect(10,35,108,20,SH110X_WHITE);
              scanningWiFi = true;
              tempoCaricamento = millis();
              display.display();
              
            }
            if (millis()-tempoCaricamento > 27){
                tempoCaricamento=millis();
                display.fillRect(11,36,LargCaricamento,18,SH110X_WHITE);
                  if (LargCaricamento<107){
                    LargCaricamento+=1;
                  }
                  //Serial.println(LargCaricamento);
                  display.display();
              }

            numWiFi = WiFi.scanComplete();    // numero di reti trovate
            if (numWiFi == WIFI_SCAN_RUNNING){
              tempoScansioneWifi = millis();
            } else if (numWiFi >= 0){    // entra nel ciclo solo se la scansione è finita
                Serial.print(F("WiFi trovate: "));
                Serial.println(numWiFi);
                st_wifi = true;
                if (numWiFi>25){
                  numWiFi=25;
              }
              display.fillRect(0,19,128,64,SH110X_BLACK);
              for (int r=0;r<numWiFi;r++){
                snprintf(listaWiFi[r], sizeof(listaWiFi[r]),"%s",WiFi.SSID(r).c_str());  //Creazione lista WIFI
                nomiWifi[r] = listaWiFi[r];
                Serial.print("SSID: ");
                Serial.println(listaWiFi[r]);
              }
              stampa_menu(numWiFi,j,nomiWifi);
              display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE); // cerchio pieno
              display.display();
            }
          }
          if (statoAttU == false && millis()-tempo>200){
              tempo=millis();
              gestBTU(numWiFi,j,nomiWifi);
              Serial.print(F("Hai selezionato: "));
              Serial.println(listaWiFi[i]);
          } else if (statoAttD == false && millis()-tempo>200){
              tempo=millis();
              gestBTD(numWiFi,j,nomiWifi);
              Serial.print(F("Hai selezionato: "));
              Serial.println(listaWiFi[i]);
          } else if (statoAttC == false && statoPrecC == true && millis()-tempo >300){
              tempo=millis();
              display.fillRect(0,0,128,64,SH110X_BLACK);   // cancella schermata
              st_wifi = false;
              schermata = 3;
              display.display();
              }

        break;






        default:
            /*Serial.print("Opzione ");
            Serial.print(lista[i]);
            Serial.println(" non disponibile.");*/
            break;
      }
      if (statoAttB == false && statoPrecB == true && millis()-tempo>300){
        tempo=millis();
        display.fillRect(0,0,128,64,SH110X_BLACK);
        i=scelta;          // indice cursore
        j=(i/5) * 5;          // indice menù
        Serial.println(i);
        Serial.println(j);
        LargCaricamento=1;
        curs_x = 7;            // posizione x cursore
        curs_y = curs_y_prec;  // riposiziono il cursore sulla posizione scelta precedentemente
        schermata=0;
        st_aura=false;
        st_canzoni=false;
        st_info=false;
        st_bro=false;
        st_wifi = false;
        audio.stopSong();

        titolo("Menu'");
        stampa_menu(len_lista,j,lista);
        display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE); // cerchio pieno
        display.display();
        return;
      }
      audio.loop();
    statoPrecB = statoAttB;
  } else if (schermata == 2){

      gest_vol();
      if (statoAttB == false && statoPrecB == true && millis()-tempo>300){
        tempo=millis();
        display.fillRect(0,0,128,64,SH110X_BLACK);
        i=0;        // indice cursore
        j=0;        // indice menù
        curs_x = 7; // posizione x cursore
        curs_y = 19; // posizione y cursore
        schermata=1;
        st_aura=false;
        st_canzoni=false;
        audio.stopSong();

        titolo("Canzoni");
        display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE); // cerchio pieno
    
        display.display();
      }

      audio.loop();
      statoPrecB = statoAttB;
  } else if (schermata == 3){
    if (st_sch3 == false){
      st_sch3 = true;
      for (int f = 0; f < 6; f++) {
        bssid_sel[f] = WiFi.BSSID(i)[f];
      }
      titolo(listaWiFi[i]);
      display.setCursor(0,17);
      display.println("livello sicurezza:");
      display.setCursor(110,17);
      display.println(WiFi.encryptionType(i));
      display.setCursor(0,27);
      display.println("segnale:");
      display.setCursor(51,27);
      display.println(WiFi.RSSI(i));
      display.setCursor(78,27);
      display.println("dbm");
      display.display();
    }
    if (statoAttC == false && statoPrecC== true&& millis()-tempo >300){
      tempo=millis();
      numWiFi = WiFi.scanNetworks();    // numero reti wifi trovate
          if (numWiFi>25){
            numWiFi=25;
          }
          for (int r=0;r<numWiFi;r++){
            Serial.println(WiFi.SSID(r));
            bssidValido = true;
            for (int f=0;f<6;f++){
              if (WiFi.BSSID(r)[f] != bssid_sel[f]){    // se i byte sono diversi...
                bssidValido = false;
                break; 
              }
            }
            if (bssidValido==true){
              Serial.print(WiFi.SSID(r));
              Serial.println(F(" : rete valida"));
              display.fillRect(47,27,30,10,SH110X_BLACK);
              display.setCursor(51,27);
              display.println(WiFi.RSSI(r));
              display.display();
              bssidValido=false;
              break;
            }
            
          }
          WiFi.scanDelete();
    }

    if (statoAttB == false && statoPrecB == true && millis()-tempo>300){
      tempo=millis();
      display.fillRect(0,0,128,64,SH110X_BLACK);
      st_sch3 = false;

      titolo(lista[WIFI]);
      curs_x = 7; // posizione x cursore
      curs_y = 19; // posizione y cursore
      j=0;
      scelta = WIFI;
      i=0;
      schermata = 1;
      bssidValido=false;
      if (millis()-tempoScansioneWifi>10000){
          scanningWiFi = false;
          WiFi.scanDelete();   // libera memoria
        }
      LargCaricamento=1;

      display.display();
    }
    statoPrecB = statoAttB;
  }
  
  statoPrecC = statoAttC;

  
// se non viene premuto un tasto per n secondi.... va in light sleep mode
if (millis()-tempo>=60000){
    Serial.println(i);
    display.clearDisplay();
    display.setCursor(10,10);
    display.print("Sleep Mode");
    display.display();

    gpio_wakeup_enable((gpio_num_t)BTU, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BTD, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BTC, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BTB, GPIO_INTR_LOW_LEVEL);

    esp_sleep_enable_gpio_wakeup();

    //display.oled_command(SH110X_DISPLAYOFF);    // spegne display
    esp_light_sleep_start();
    //display.oled_command(SH110X_DISPLAYON);   // riaccendo display
    tempo=millis();
    display.clearDisplay();
    if (schermata == 0){
      j=(i/5) * 5;          // indice menù

      display.fillCircle(curs_x, curs_y, 2, SH110X_WHITE); // cerchio pieno
      titolo("Menu'");
      stampa_menu(len_lista,j,lista);
    } else if (schermata == 1){

        LargCaricamento=1;

        st_aura=false;
        st_canzoni=false;
        st_info=false;
        st_bro=false;
        st_wifi = false;

        titolo(lista[scelta]);
        j=(i/5) * 5;
        display.display();


        if (millis() - tempoScansioneWifi > 10000 && scelta==WIFI) {
            scanningWiFi = false;
            WiFi.scanDelete();
        }
    } else if (schermata == 2){
        display.setCursor(5,19);
        display.print("Volume:");
        display.setCursor(48,19);
        display.print(volume);
        st_volume();
        titolo(canzoni[i]);
    } else if (schermata == 3){
        st_sch3=false;
    }
    display.display();
  }



}
