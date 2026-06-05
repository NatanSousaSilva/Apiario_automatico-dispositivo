#include <DHT.h>
#include "HX711.h"

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

#include <Preferences.h>
#include <ctime>

class Comunicacao_App {
public: 
  Comunicacao_App() : server(80) {}

  void begin(int _btn, String _url) {
    this->btn = _btn;
    this->url = _url

    pinMode(btn, INPUT_PULLUP);

    server.on("/", HTTP_GET, [this]() { this->pagina_conexao(); });
    server.on("/conectar_wifi", HTTP_GET, [this]() { this->conectar_wifi(); });
    server.on("/horario", HTTP_GET, [this]() {this->processar_horario();});
    
    server.begin();
 
    if (this->modo_sta() == false) {
      this->modo_ap();
    }
  }

  ///////

  String gerenciar_servidor() {
    int estado_btn = digitalRead(btn);

    if (estado_btn == LOW) { 
      if (validacao_t == false) {
        t = millis();
        validacao_t = true; 
      }
      if (validacao_t == true) {
        if (millis() - t >= 10000) { 
          validacao_t = false;
          this->modo_ap(); 
        }
      }
    }
    if (estado_btn == HIGH) { 
      validacao_t = false;
    }

    server.handleClient();
    
    String msg = horario;
    horario = "";
    
    return msg;
  }

  ///////

  void enviar_dados_sensor(String dados, char sensor){
    String json = "{\"" + sensor + "\":\"" + dados + "\"}";

    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(json); 
    
  }
  
  ///////
  

private:
  const char* ssid_ap = "Apiario";
  const char* password_ap = "12345678";
  const char* chave = "000001";

  String ssid_st;
  String password_st;
  
  String url;

  ///

  int btn;
  unsigned long t; 
  String horario = "";

  bool operando_modo_ap = false;
  bool validacao_t = false; 

  ///

  Preferences prefs;
  WebServer server;
  HTTPClient http;
  WiFiClient client;

  ///

  bool modo_sta() {
    prefs.begin("config", false);
    ssid_st = prefs.getString("ssid_st", "");
    password_st = prefs.getString("password_st", "");
    prefs.end();
      
    if (ssid_st != "") {
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid_st.c_str(), password_st.c_str());
  
      unsigned long tempoInicio = millis();
      while (WiFi.status() != WL_CONNECTED && (millis() - tempoInicio < 5000)) {
        delay(100);
      }
  
      if (WiFi.status() == WL_CONNECTED) {        
        http.begin(client, url);
        
        operando_modo_ap = false;
        return true;          
      }
    }
    return false;
  }

  ///////

  void modo_ap() {
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.disconnect(true);
      http.end();
    }
    operando_modo_ap = true;
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid_ap, password_ap);

    Serial.println("Modo AP Iniciado. IP: " + WiFi.softAPIP().toString());
  }

  ///////

  void conectar_wifi() {
    if (operando_modo_ap){
      if (server.hasArg("ssid") && server.hasArg("password")) {
        ssid_st = server.arg("ssid");
        password_st = server.arg("password");
  
        prefs.begin("config", false);
        prefs.putString("ssid_st", ssid_st);
        prefs.putString("password_st", password_st);
        prefs.end();
  
        server.send(200, "text/html", "Dados recebidos! Tentando conectar...");
        delay(1000);
  
        WiFi.softAPdisconnect(true);
        operando_modo_ap = false;
  
        this->modo_sta();
      } else {
        server.send(200, "text/html", "Faltam dados (SSID ou senha)");
      }
    }
    else{
      server.send(404, "text/html", "Not Found");
    }
  }

  ///////

  void processar_horario(){
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"erro\":\"Corpo da requisicao vazio\"}");
      return;
    }

    String json = server.arg("plain"); 
    int pos_data = json.indexOf("202"); 

    if (pos_data != -1) horario = pos_data.substring(0, 19); // EX:2026-06-04T15:15:14.000Z
  }
  
  ///////

  void pagina_conexao() {
    if (operando_modo_ap == true){
      server.send(200, "text/html", html_home);
    }
    else {
      server.send(404, "text/html", "Not Found");
    }
  }

  ///////

  const char html_home[] = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <title>Configurar Wi-Fi</title>
    </head>
    <body>
      <h2>Conectar dispositivo</h2>
      <form action="/conectar_wifi" method="GET">
        <label>Rede:</label><br>
        <input type="text" name="ssid"><br><br>
        <label>Senha:</label><br>
        <input type="password" name="password"><br><br>
        <input type="submit" value="Enviar">
      </form>
    </body>
    </html>
  )rawliteral";
};

class Sensores{
public:
  Sensores() : dht(18, DHT11){};

  void begin(int _sensor_ruido, int pin_dt_balanca, int pin_sck_balanca){
    sensor_ruido = _sensor_ruido;

    pinMode(sensor_ruido, INPUT);

    ruido_ambiente = analogRead(sensor_ruido);
    
    balanca.begin(pin_dt_balanca, pin_sck_balanca);
    balanca.set_scale();
    banlaca.tare();
  }

  float ler_humidade(){
    return dht.readHumidity();
  }

  float ler_temperatura(){
    return dht.readTemperature();
  }

  long ler_peso(){
    return balanca.get_units(10);
  }

  int ler_ruido(){
    int ruido = analogRead(sensor_ruido);
    return ruido - ruido_ambiente;
  }

  void set_ruido_ambiente(int _ruido_ambiente){
    ruido_ambiente = _ruido_ambiente;
  }

private:
  DHT dht; // pino, versao
  HX711 balanca;

  int ruido_ambiente;
  int sensor_ruido;
};

/////////////



Comunicacao_App canal;
Sensores sensores;

String t_atual[4]; // ruido, humidade, temperatura, peso;

bool comparar_horarios(String horario, String &t_atual, int diferenca_segundos);

void setup() {
  sensores.begin(2, 3, 4);
  canal.begin(5, "http://apiario/dados");
}

void loop() {
  String horario = canal.gerenciar_servidor();

  if (horario != ""){
    if (comparar_horarios(horario, t_atual[0])){
      canal.enviar_dados_sensor(String(sensores.ler_ruido()), 'r', 30);
    }
    if (comparar_horarios(horario, t_atual[1])){
      canal.enviar_dados_sensor(String(sensores.ler_humidade()), 'h', 43200);
    }
    if (comparar_horarios(horario, t_atual[2])){
      canal.enviar_dados_sensor(String(sensores.ler_temperatura()), 't', 43200);
    }
    if (comparar_horarios(horario, t_atual[3])){
      canal.enviar_dados_sensor(String(sensores.ler_peso()), 'p', 604800);
    }
    
  }
}

bool comparar_horarios(String horario, String &t_a, int diferenca_segundos) {
  int ano1     = horario.substring(0, 4).toInt();
  int mes1     = horario.substring(5, 7).toInt();
  int dia1     = horario.substring(8, 10).toInt();
  int hora1    = horario.substring(11, 13).toInt();
  int minuto1  = horario.substring(14, 16).toInt();
  int segundo1 = horario.substring(17, 19).toInt();

  int ano2     = t_a.substring(0, 4).toInt();
  int mes2     = t_a.substring(5, 7).toInt();
  int dia2     = t_a.substring(8, 10).toInt();
  int hora2    = t_a.substring(11, 13).toInt();
  int minuto2  = t_a.substring(14, 16).toInt();
  int segundo2 = t_a.substring(17, 19).toInt();

  struct tm t1 = {0};
  t1.tm_year = ano1 - 1900; 
  t1.tm_mon  = mes1 - 1;
  t1.tm_mday = dia1;
  t1.tm_hour = hora1;
  t1.tm_min  = minuto1;
  t1.tm_sec  = segundo1;

  struct tm t2 = {0};
  t2.tm_year = ano2 - 1900;
  t2.tm_mon  = mes2 - 1;
  t2.tm_mday = dia2;
  t2.tm_hour = hora2;
  t2.tm_min  = minuto2;
  t2.tm_sec  = segundo2;

  time_t segundos1 = mktime(&t1);
  time_t segundos2 = mktime(&t2);

  long diferenca_calculada = abs(segundos1 - segundos2);
  
  if (diferenca_calculada <= diferenca_segundos){
    t_a = horario;
    return true
  }
  return false;
}
