
/**
 * Created by K. Suwatchai (Mobizt)
 * 
 * Email: k_suwatchai@hotmail.com
 * 
 * Github: https://github.com/mobizt
 * 
 * Copyright (c) 2021 mobizt
 *
*/

/** This example will show how to authenticate using
   the legacy token or database secret with the new APIs (using config and auth data).
*/
#if defined(ESP32)
#include <WiFi.h>
#include <FirebaseESP32.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <SoftwareSerial.h>
#endif

//Pinos de comunicacao serial com a ST Ncleo
#define Pin_ST_NUCLEO_RX    5  //Pino D1 da placa Node MCU
#define Pin_ST_NUCLEO_TX    4  //Pino D2 da placa Node MCU

SoftwareSerial SSerial(Pin_ST_NUCLEO_RX, Pin_ST_NUCLEO_TX);

//Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

#define RESTART_SERIAL_COMMAND "S\r\n"

/* 1. Define the WiFi credentials */
#define WIFI_SSID "SSID 2G"
#define WIFI_PASSWORD "password"

WiFiServer server(80);

// Conexao com Firebase
#define DATABASE_URL "https://se2021-c4bb1-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "secret"

/* 3. Define the Firebase Data object */
FirebaseData fbdo;

/* 4, Define the FirebaseAuth data for authentication data */
FirebaseAuth auth;

/* Define the FirebaseConfig data for config data */
FirebaseConfig config;

void setup()
{
  // Define a velocidade das portas seriais
  Serial.begin(115200);
  SSerial.begin(115200);

  // Conexao com a rede wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();

  server.begin(); //INICIA O SERVIDOR PARA RECEBER DADOS NA PORTA DEFINIDA EM "WiFiServer server(porta);"
  Serial.println("Servidor iniciado"); //ESCREVE O TEXTO NA SERIAL
  Serial.print("IP para se conectar ao NodeMCU: "); 
  Serial.print("http://"); 
  Serial.println(WiFi.localIP()); //ESCREVE NA SERIAL O IP RECEBIDO DENTRO DA REDE SEM FIO (O IP NESSA PRTICA  RECEBIDO DE FORMA AUTOMTICA)

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the database URL and database secret(required) */
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  Firebase.reconnectWiFi(true);

  /* Initialize the library with the Firebase authen and config */
  Firebase.begin(&config, &auth);

  // Zera a informacao no firebase
  Firebase.setInt(fbdo, "/carro/vel", 0);
  // Manda caracter 'S' via serial para o nucleo. Comando de redefinicao da velocidade.
  SSerial.write(RESTART_SERIAL_COMMAND);
}

void loop()
{
  if (SSerial.available()) {
    char array[8];
    String s = SSerial.readString();
    s.toCharArray(array, 8);
    Serial.write("Recebendo valor do Nucleo\r\n");
    Serial.write(array);

    int vel = s.toInt();
    Firebase.setInt(fbdo, "/carro/vel", vel);
  }

  //VERIFICA SE ALGUM CLIENTE ESTA CONECTADO NO SERVIDOR
  WiFiClient client = server.available();
  if (!client) { //SE NAO EXISTIR CLIENTE CONECTADO
    return; //REEXECUTA O PROCESSO AT QUE ALGUM CLIENTE SE CONECTE AO SERVIDOR
  }

  Serial.println("Nova conexão HTTP!"); //ESCREVE O TEXTO NA SERIAL
  while (!client.available()) {
    delay(1); //INTERVALO DE 1 MILISEGUNDO
  }

  // Cdigo HTML do web server
  String request = client.readStringUntil('\r'); //FAZ A LEITURA DA PRIMEIRA LINHA DA REQUISIO
  Serial.println(request); //ESCREVE A REQUISIO NA SERIAL
  client.flush(); //AGUARDA AT QUE TODOS OS DADOS DE SADA SEJAM ENVIADOS AO CLIENTE

  if (request.indexOf("POST /reset HTTP") >= 0) {
    // SE A REQUEST DO CLIENTE FOR O COMANDO DO BOTÃO DE RESET

    Serial.println("Velocidada redefinida");
    // Manda informacao para o firebase. Resetando velocidade
    Firebase.setInt(fbdo, "/carro/vel", 0);

    // Manda a caracter 'S' via serial para o nucleo
    SSerial.write(RESTART_SERIAL_COMMAND);

    // Responde ao cliente com redirecionamento para o caminho raiz /
    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /");
  } else if (request.indexOf("GET /vel HTTP") >= 0) {
    // SE A REQUEST DO CLIENTE FOR UM GET PARA PEGAR A VELOCIDADE ATUAL

    // Baixa o valor da velocidade do BD no Firebase
    Firebase.getInt(fbdo, "/carro/vel");

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("");
    client.println(fbdo.to<int>()); // Body da resposta é o valor da velocidade pega do banco
  } else if (request.indexOf("GET / HTTP") >= 0) {
    // SE A REQUEST DO CLIENTE FOR O CARREGAMENTO DA PAGINA

    // Solicita o valor da velocidade do BD no Firebase
    Firebase.getInt(fbdo, "/carro/vel");

    client.println("HTTP/1.1 200 OK"); //ESCREVE PARA O CLIENTE A VERSO DO HTTP
    client.println("Content-Type: text/html"); //ESCREVE PARA O CLIENTE O TIPO DE CONTEDO(texto/html)
    client.println("");
    client.println("<!DOCTYPE HTML>"); //INFORMA AO NAVEGADOR A ESPECIFICAO DO HTML
    client.println("<html>"); //ABRE A TAG "html"

    // Script para solicitar a velocidade a cada 2 segundos e atualizar na página sem precisar dar refresh.
    client.println("\
        <script>\
          setInterval(function(){\
            fetch('/vel').then(response => {\
              if(response.ok) {\
                response.json().then(function(value){\
                  var velSpan = document.getElementById('vel');\
                  velSpan.textContent = value\
                });\
              }\
            })\
          }, 2000);\
        </script>\
        ");

    // Conteúdo principal da página
    client.println("\
        <h1 style='text-align: center;'>Velocidade do carro</h1>\
        <div style='text-align: center;'>\
          <img src='data:image/png;base64,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'>\
          <div style='margin: 20px 0; font-size: 3em;'>Velocidade: <span id='vel'>");
    client.print(fbdo.to<int>());
    client.println("</span>u/s</div>\
          <form action='/reset' method='post'>\
            <button style='background: #2a2b2c; border-radius: 8px; padding: 12px; cursor: pointer; color: #fff; border: none;'>Reiniciar</button>\
          </form>\
        </div>\
        ");

    client.println("</html>"); //FECHA A TAG "html"
  } else {
    client.println("HTTP/1.1 404 Not Found");
  }

  delay(1); //INTERVALO DE 1 MILISEGUNDO
  Serial.println("Conexão HTTP encerrada"); //ESCREVE O TEXTO NA SERIAL
  Serial.println(""); //PULA UMA LINHA NA JANELA SERIAL
}
