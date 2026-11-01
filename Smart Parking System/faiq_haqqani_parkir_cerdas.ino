#include <stdlib.h>


struct Simpul {
  int simpul;
  struct Simpul* next;
};

struct Graf {
  int jumlahSimpul;
  struct Simpul** adjLists;
  int berarah;
};


int ledPins[5] = {2, 3, 4, 5, 6};
const int pingPin = 7; 
const int buzzerPin = 8;

struct Graf* grafMobil;
struct Graf* grafFSM; 


const int AMAN = 0;
const int BAHAYA = 1;
int currentState = AMAN;


struct Simpul* buatSimpul(int v) {
  struct Simpul* simpulBaru = (struct Simpul*)malloc(sizeof(struct Simpul));
  simpulBaru->simpul = v;
  simpulBaru->next = NULL;
  return simpulBaru;
}

struct Graf* buatGraf(int simpul, int berarah) {
  struct Graf* graf = (struct Graf*)malloc(sizeof(struct Graf));
  graf->jumlahSimpul = simpul;
  graf->berarah = berarah;
  graf->adjLists = (struct Simpul**)malloc(simpul * sizeof(struct Simpul*));

  for (int i = 0; i < simpul; i++) {
    graf->adjLists[i] = NULL;
  }
  return graf;
}

void tambahEdge(struct Graf* graf, int src, int dest) {
  struct Simpul* simpulBaru = buatSimpul(dest);
  simpulBaru->next = graf->adjLists[src];
  graf->adjLists[src] = simpulBaru;
}


bool checkEdge(struct Graf* graf, int src, int dest) {
  struct Simpul* temp = graf->adjLists[src];
  while (temp) {
    if (temp->simpul == dest) {
      return true;
    }
    temp = temp->next;
  }
  return false;
}



void matikanSemuaLED() {
  for (int i = 0; i < 5; i++) digitalWrite(ledPins[i], LOW);
}


void bfs(struct Graf* graf, int mulaiSimpul) {
  matikanSemuaLED();

  int* dikunjungi = (int*)malloc(graf->jumlahSimpul * sizeof(int));
  for (int i = 0; i < graf->jumlahSimpul; i++) dikunjungi[i] = 0;

  int* antrian = (int*)malloc(graf->jumlahSimpul * sizeof(int));
  int depan = 0;
  int belakang = 0;

  dikunjungi[mulaiSimpul] = 1;
  antrian[belakang++] = mulaiSimpul;

  while (depan < belakang) {
    int simpulSaatIni = antrian[depan++];

    digitalWrite(ledPins[simpulSaatIni], HIGH);
    Serial.print("Booting Node: "); Serial.println(simpulSaatIni);
    delay(800);

    struct Simpul* temp = graf->adjLists[simpulSaatIni];
    while (temp) {
      int tetangga = temp->simpul;
      if (!dikunjungi[tetangga]) {
        dikunjungi[tetangga] = 1;
        antrian[belakang++] = tetangga;
      }
      temp = temp->next;
    }
  }
  free(antrian);
  free(dikunjungi);
}

void setup() {
  Serial.begin(9600);

  // Init Pins
  for (int i = 0; i < 5; i++) pinMode(ledPins[i], OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  grafMobil = buatGraf(5, 1); // 5 Node
  tambahEdge(grafMobil, 0, 1);
  tambahEdge(grafMobil, 0, 2);
  tambahEdge(grafMobil, 1, 3);
  tambahEdge(grafMobil, 2, 4);

  grafFSM = buatGraf(2, 1);
  tambahEdge(grafFSM, AMAN, BAHAYA);
  tambahEdge(grafFSM, BAHAYA, AMAN);
  tambahEdge(grafFSM, AMAN, AMAN);
  tambahEdge(grafFSM, BAHAYA, BAHAYA);

  Serial.println("Sistem Mobil Siap.");
  Serial.println("Ketik: 'B' untuk Booting (BFS) atau 'P' untuk Mode Parkir.");
}

void loop() {
  if (Serial.available()) {
    char command = Serial.read();

    if (command == 'B' || command == 'b') {
      Serial.println("\n--- MODE BOOTING (BFS) ---");
      bfs(grafMobil, 0);
      Serial.println("Sistem Online.");
    }

    else if (command == 'P' || command == 'p') {
      Serial.println("\n--- MODE PARKIR (FSM) ---");
      Serial.println("Letakkan objek < 102cm untuk trigger BAHAYA.");
      Serial.println("Jauhkan objek > 250cm untuk KELUAR mode.");

      bool parkingmode = true;

      while (parkingmode == true) { 
        long duration, cm;
        pinMode(pingPin, OUTPUT);
        digitalWrite(pingPin, LOW); delayMicroseconds(2);
        digitalWrite(pingPin, HIGH); delayMicroseconds(5);
        digitalWrite(pingPin, LOW);

        pinMode(pingPin, INPUT);
        duration = pulseIn(pingPin, HIGH);
        cm = duration * 0.034 / 2;

        if (cm > 250) {
          Serial.println("Objek menjauh. Keluar Mode Parkir...");
          matikanSemuaLED();
          noTone(buzzerPin);
          parkingmode = false;
          break;
        }

      
        int targetState;
        if (cm < 102) targetState = BAHAYA; 
        else targetState = AMAN;

      
        if (checkEdge(grafFSM, currentState, targetState)) {
          currentState = targetState;

          if (currentState == BAHAYA) {

            for (int i = 0; i < 5; i++) digitalWrite(ledPins[i], HIGH);
            tone(buzzerPin, 1000);
            delay(100);
            for (int i = 0; i < 5; i++) digitalWrite(ledPins[i], LOW);
            noTone(buzzerPin);
            delay(100);
          } else {
            // Indikasi Aman
            matikanSemuaLED();
            digitalWrite(ledPins[0], HIGH);
            delay(200);
          }
        }
      }
    }
  }
}