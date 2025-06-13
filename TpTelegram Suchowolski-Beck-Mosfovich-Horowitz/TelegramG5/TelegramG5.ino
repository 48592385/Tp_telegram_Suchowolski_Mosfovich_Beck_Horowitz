// Apellidos: Beck,Suchowolski,Horowitz,Mosfovich, Grupo:5
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <U8g2lib.h>

// === Pines y objetos ===
#define DHTPIN 23
#define DHTTYPE DHT11
#define BOTON_MAS 35
#define BOTON_MENOS 34
#define LED_ALARMA 25

DHT dht(DHTPIN, DHTTYPE);    //decalaras que tipo de dht es 
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);//para configurar las medias del dysplay o la pnatallita

// === WiFi y Telegram ===
const char* ssid = "Redmi Note 12 5G";//red a la que nos vamos a conectar(celu de pucho)
const char* password = "bifs4085";
#define BOTtoken "7555765954:AAHLXTZdBdhP7iuAfUzF20HmTXJIZxbTbng"//contraseña del bot de telegram
#define CHAT_ID "7915810064" //el di de nuestro usuario en telegram
WiFiClientSecure client;//llama a la libreria 
UniversalTelegramBot bot(BOTtoken, client);//configura el bot para que mande y reciba mensajes

// === Variables globales ===
float tempActual = 0; //temperatura actual
int VU = 28; // valor umbral inicial//el valor umbral con el que incicias y el cual vos podes ir modificando
bool alertaEnviada = false;//indica que la alerta fue enviada, del bot de telegram a nosotros
volatile bool debeEnviarAlerta = false;//es una señal que activa el mensaje de telegram, que sea volatil es que puede ir cambiando de un momento a otro
volatile bool debeEnviarNormal = false;//lo mismo, pero avisa que volvio a la nornmalidad. El bot de telegram nos avisa a nosotros
unsigned long lastTempMillis = 0;//la variable Tempmilis va contando el tiempo desde la ultima vez que midio la temperatura, cada 5 segundo se renueva, osea mide cada 5 segundos

// === Estados ===
enum Estado { P1, ESPERA1, CODE1, ESPERA2, CODE2, ESPERA3, P2, SUMA, RESTA, ESPERA4 };//maquina de estados, delcaramos los estados
Estado estado = P1;//en que parte de la maquina de estados comienza todo esto, osea en p1 mostrando la temepratura actual
unsigned long codeStartTime = 0;//una vez que apretaste el boton, empieza a contar hasta 5 para que el tiempo de la secuencia no sea ilimitado 

// === Funciones ===
void mostrarPantallaPrincipal();//define las funciones 
void mostrarPantallaUmbral();

void setup() {
  Serial.begin(115200);//inicia el serial monitor
  dht.begin();
  u8g2.begin();

  pinMode(BOTON_MAS, INPUT_PULLUP);//define botones
  pinMode(BOTON_MENOS, INPUT_PULLUP);
  pinMode(LED_ALARMA, OUTPUT);
  digitalWrite(LED_ALARMA, LOW);

  WiFi.begin(ssid, password);//el sp32 se concta al wifi, con su red y contraseña
  while (WiFi.status() != WL_CONNECTED) {//se queda esperando a que el sp32 se conecte al wifi
    Serial.print(".");//mientras espera, van apareciendo puntos, con un delay de 100. 
    delay(100);
  }
  Serial.println("WiFi connected");//cuando el sp32 se conecta al wifi, muestra que se concetos. 
  client.setInsecure();//te lo pide telegram
  bot.sendMessage(CHAT_ID, "Bot iniciado", "");//el bot de telegram te manda un mensaje diciendote que incio

  xTaskCreatePinnedToCore(tareaTelegram,"Telegram", 8192, NULL, 1, NULL,1);//se encarga de que el bot pueda leer los mensajes y enviar las alertas
  xTaskCreatePinnedToCore(tareaEstados, "Estados",   8192, NULL, 1, NULL,0);//ejecuta la maquina de estados y activa los sensores y la pantalla led
}

void loop() {
  // no se usa
}

// === Tarea: Telegram ===
void tareaTelegram(void *pvParam) {//
  for (;;) {//quiere decri que lo hace infinitamente 
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);//cuando el bot recibe un mensaje lo guarda en la variable bot.last_menssage_recived
    while (numNewMessages) { //mientras haya un nuevo mensaje
      for (int i = 0; i < numNewMessages; i++) {//por cada nuevo mensaje, i va a estar keyendoi cada uno. 
        String msg = bot.messages[i].text;//la variable i se guarda en bot.menssages(es una estructura para guardar mensajes) y luego esto se guarda en MSG
        if (msg == "TEMPERATURA") {//si el mensaje es la temepratura
          bot.sendMessage(CHAT_ID, "Temp actual: " + String(tempActual, 1) + " C", "");//el bot manda el mensaje de la temperatura actuar a telegram. El chat id quiere decri que le estamos 
          //dando el numero de nuestro chat. Osea le decrimos que nos lo mande a nosotros  
        }
      }
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);//cuando el bot recibe un mensaje, lo guarda en la variable  bot.last_menssage_recived
    }

    // Enviar alerta si fue marcada
    if (debeEnviarAlerta) {//si la alerta es un true(osea si supero el valor umabral)
      bot.sendMessage(CHAT_ID, "⚠️ ALERTA: Temp=" + String(tempActual, 1) + "C superó el umbral VU=" + String(VU), "");//el bot te avisa que se supero el valor umbral, porque mas abajo se lo dicen
      debeEnviarAlerta = false;//ahora vuelve a la normalidad. 
    }

    // Enviar normalización si fue marcada
    if (debeEnviarNormal) {//si la temperatura es menor  al valor umbral,
      bot.sendMessage(CHAT_ID, "✅ Temp normalizada. Temp=" + String(tempActual, 1) + "C", "");//el bot 
      debeEnviarNormal = false;
    }

    delay(1000);
  }
}

// === Tarea: Máquina de estados ===
void tareaEstados(void *pvParam) {
  for (;;) {
    unsigned long ms = millis();//es para no usar delay. Determina cuanto tiempo va a estar prendido el sp32 en milisegundos

    // Lectura cada 5s
    if (ms - lastTempMillis >= 5000) {//si pasaron 5 segundos 
      tempActual = dht.readTemperature();//el dht va a leer la temperatura actual Y dicha temperatura leida se gurda en tempActual. 
      lastTempMillis = ms;//el ultimo tiempo que se registro se guarda en ms 
      if (!isnan(tempActual)) {//que si el sensor de dht no puede registrar la temperatura, pero si la registra 
        if (tempActual > VU && !alertaEnviada) {// si la temepratura actual es mayor al umbral y la alerta no fue enviada
          digitalWrite(LED_ALARMA, HIGH);//se prende el led, osea porque la temp actual supero el valor umbral
          debeEnviarAlerta = true;//se envia la alerta, de que el bot tiene que mandar el mensaje
          alertaEnviada = true;//avisa que la alerta fue enviada 
        } else if (tempActual <= VU && alertaEnviada) {//pero si la temp actual es menor al valor umbral y la alarta fue enviada. 
          digitalWrite(LED_ALARMA, LOW);//se apaga el led
          debeEnviarNormal = true;//se manda la alerta de esto
          alertaEnviada = false;//se avisa que la alerta fue enviada 
        }
      }
    }

    // Máquina de estados
    switch (estado) {//maquina de estados
      case P1://primer estado, en este estado se muestra la pantalla principal
        mostrarPantallaPrincipal();//se muestra la temp actual y el valor umbral
        if (digitalRead(BOTON_MAS) == LOW) {//si presionas el boton 1 osea el que suma
          codeStartTime = ms;//empieza a contar los 5 segundos para reiniciar o no la secuancia
          estado = ESPERA1;//va a caso espera 
        }
        break;
      case ESPERA1:
        if (digitalRead(BOTON_MAS) == HIGH) {//si soltas el boton 1 
          estado = CODE1;//pasas a cod1
        }
        break;
      case CODE1:
        if (ms - codeStartTime > 5000) {//si pasaron 5 segundos
          estado = P1;//volves a la pantalla principal
          break;
        }
        if (digitalRead(BOTON_MENOS) == LOW) {//si presionas el boton 2 
          codeStartTime = ms;//cuanta 5 segundos
          estado = ESPERA2;//vas a espera2
        }
        break;
      case ESPERA2:
        if (digitalRead(BOTON_MENOS) == HIGH) {//si se suelta el boton
          estado = CODE2;//se va a cod2
        }
        break;
      case CODE2:
        if (ms - codeStartTime > 5000) {//si se cumplen los 5 segundos
          estado = P1;//volves a la pantalla principal
          break;
        }
        if (digitalRead(BOTON_MAS) == LOW) {//si preisonas el boton 1 
          codeStartTime = ms;//se comeinza a contar los 5 seg
          estado = ESPERA3;//vas a espera 3 
        }
        break;
      case ESPERA3:
        if (digitalRead(BOTON_MAS) == HIGH) {//si soltas el boton 1 
          estado = P2;//vas a p2
        }
        break;
      case P2:
        mostrarPantallaUmbral();//pantalla donde podes modoficar el valor umbral
        if (digitalRead(BOTON_MENOS) == LOW && digitalRead(BOTON_MAS) == LOW) {//cuando presionas los dos botones al mismo tiempo
          estado = ESPERA4;//pasar a espera 4, que es cuando espera a que sueltes los dos botones
        } else if (digitalRead(BOTON_MENOS) == LOW) {//pero si presionas solo el boton 2
          estado = RESTA;//VAS A RESTA, QUE ES DONDE RESTAS EL VALOR UMBRAL
        } else if (digitalRead(BOTON_MAS) == LOW) {//pero si presionas el boton 1 
          estado = SUMA;//vas a estado suma que es en donde se suma el valor umbral
        }
        break;
      case SUMA://donde se suma elk valor umbral
       if(digitalRead(BOTON_MENOS) == LOW){ // si toca el boton menos antes de tocar soltar el boton mas va a espera 4 y despues va a pantalla 1
       estado = ESPERA4;
        }
        if (digitalRead(BOTON_MAS) == HIGH) {//si soltas el boton 
          VU++;//el valor umbral suma 1 
          estado = P2;//volves a la pantalla
        }
        break;
      case RESTA://donde se resta el valor umbral
       if(digitalRead(BOTON_MAS) == LOW){ // si toca el boton mas antes de soltar el boton menos va a espera 4 y despues cuando suelta los dos va a pantalla 1
        estado = ESPERA4;
        }
        if (digitalRead(BOTON_MENOS) == HIGH) {//cuando soltas el booton 2 
          VU--;//el valor umbral se resta 1 
          estado = P2;//vovles a la pantalla
        }
        break;
      case ESPERA4://en este caso 
        if (digitalRead(BOTON_MAS) == HIGH && digitalRead(BOTON_MENOS) == HIGH) {//cuando soltas los dos botones previamente presionados
          estado = P1;//volves a la pantalla principal, osea la cual te meustra la temepratura actual y el valor umbral 
        }
        break;
    }
  }
}

// === Funciones ===
void mostrarPantallaPrincipal() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 12, "Temp:");
  u8g2.setCursor(60, 12); u8g2.print(tempActual, 1);
  u8g2.drawStr(98, 12, "C");
  u8g2.drawStr(0, 40, "Umbral:");
  u8g2.setCursor(60, 40); u8g2.print(VU);
  u8g2.sendBuffer();
}

void mostrarPantallaUmbral() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 16, "AJUSTE UMBRAL");
  u8g2.drawStr(15, 40, "VU:");
  u8g2.setCursor(60, 40); u8g2.print(VU);
  u8g2.sendBuffer();
}
