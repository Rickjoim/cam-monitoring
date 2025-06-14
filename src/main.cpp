#include <WiFi.h>
#include <ArduinoHA.h>
#include "DHTesp.h"
#include <HTTPClient.h>
#include "secrets.h"

// --- PINOS ---
#define DHT_PIN          13
#define LED_BLUE_PIN     18
#define LED_GREEN_PIN    19
#define LED_RED_PIN      21
#define PIR_PIN          23 

// --- CONFIGURAÇÕES GERAIS ---
#define PUBLISH_PERIOD   10000 
unsigned long lastTime = 0;

// --- VARIÁVEIS DE CONTROLE ---
unsigned long lastMotionTime = 0;
const long motionCooldown = 5000; 
bool lastMotionState = false;     

// --- OBJETOS E CLIENTES ---
DHTesp dhtSensor;
WiFiClient client;
HADevice device("ESP32_Wokwi_01");
HAMqtt mqtt(client, device);

// --- ENTIDADES DO HOME ASSISTANT ---
HASwitch led_red("esp32wokwi01_led_red");
HASwitch led_green("esp32wokwi01_led_green");
HASwitch led_blue("esp32wokwi01_led_blue");
HASensor dhtSensorTemp("esp32wokwi01_temperature");
HASensor dhtSensorHumi("esp32wokwi01_humidity");
HABinarySensor motionSensor("esp32wokwi01_motion");


// Função que substitui a biblioteca UrlEncode
String urlEncode(String str) {
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encodedString += '+';
        } else if (isalnum(c)) {
            encodedString += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
    }
    return encodedString;
}

// Função para enviar a notificação via WhatsApp
void sendWhatsAppMessage(String message) {
    if (millis() - lastMotionTime < motionCooldown) {
        Serial.println("Cooldown ativo. Mensagem não enviada para evitar spam.");
        return;
    }
    lastMotionTime = millis();

    String urlEncodedMessage = urlEncode(message);
    String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&text=" + urlEncodedMessage + "&apikey=" + apikey;

    HTTPClient http;
    http.begin(url);
    http.setConnectTimeout(5000); 

    Serial.println("Enviando notificação para o WhatsApp...");
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        Serial.print("Código de resposta HTTP: ");
        Serial.println(httpResponseCode);
    } else {
        Serial.print("Erro na requisição HTTP: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

// Funções de callback para os LEDs
void onRedSwitchCommand(bool state, HASwitch* sender) {
    Serial.print("LED red turning ");
    Serial.println((state ? "ON" : "OFF"));
    digitalWrite(LED_RED_PIN, (state ? HIGH : LOW));
    sender->setState(state);
}

void onGreenSwitchCommand(bool state, HASwitch* sender) {
    Serial.print("LED green turning ");
    Serial.println((state ? "ON" : "OFF"));
    digitalWrite(LED_GREEN_PIN, (state ? HIGH : LOW));
    sender->setState(state);
}

void onBlueSwitchCommand(bool state, HASwitch* sender) {
    Serial.print("LED blue turning ");
    Serial.println((state ? "ON" : "OFF"));
    digitalWrite(LED_BLUE_PIN, (state ? HIGH : LOW));
    sender->setState(state);
}


void setup() {
    Serial.begin(115200);
    Serial.print("Starting ");
    dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

    pinMode(LED_RED_PIN, OUTPUT);
    digitalWrite(LED_RED_PIN, LOW);
    pinMode(LED_GREEN_PIN, OUTPUT);
    digitalWrite(LED_GREEN_PIN, LOW);
    pinMode(LED_BLUE_PIN, OUTPUT);
    digitalWrite(LED_BLUE_PIN, LOW);
    pinMode(PIR_PIN, INPUT_PULLUP);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nConnected to the network");

    device.setName("ESP32-Wokwi-01");
    device.setManufacturer("Rickson Lima");
    device.setModel("ESP32-Wokwi");
    device.setSoftwareVersion("1.3.0"); // Versão incrementada
    device.enableSharedAvailability();
    device.setAvailability(true); 
    device.enableLastWill();

    led_red.onCommand(onRedSwitchCommand);
    led_red.setName("LED Vermelho");
    led_green.onCommand(onGreenSwitchCommand);
    led_green.setName("LED Verde");
    led_blue.onCommand(onBlueSwitchCommand);
    led_blue.setName("LED Azul");
    
    dhtSensorTemp.setName("Temperatura");
    dhtSensorTemp.setDeviceClass("temperature");
    dhtSensorTemp.setUnitOfMeasurement("°C");
    dhtSensorTemp.setIcon("mdi:temperature-celsius");
    dhtSensorHumi.setName("Umidade");
    dhtSensorHumi.setDeviceClass("humidity");
    dhtSensorHumi.setUnitOfMeasurement("%");
    dhtSensorHumi.setIcon("mdi:water-percent");

    motionSensor.setName("Sensor de Movimento");
    motionSensor.setDeviceClass("motion");

    mqtt.begin(BROKER_ADDR, BROKER_USERNAME, BROKER_PASSWORD);
}

void loop() {
    mqtt.loop();

    // LÓGICA CORRIGIDA ABAIXO
    bool currentMotionState = digitalRead(PIR_PIN);
    
    // Compara o estado ATUAL com o ÚLTIMO estado conhecido
    if (currentMotionState != lastMotionState) {
        // Se mudou, atualiza o Home Assistant
        motionSensor.setState(currentMotionState);

        if (currentMotionState == true) { // Se o estado mudou para "com movimento"
            Serial.println("Movimento detectado!");
            digitalWrite(LED_RED_PIN, HIGH);
            led_red.setState(true); // Atualiza o estado do switch no HA
            sendWhatsAppMessage("🚨 Alerta! Movimento detectado pelo sensor da sala.");
        } else { // Se o estado mudou para "sem movimento"
            Serial.println("Movimento cessou.");
            digitalWrite(LED_RED_PIN, LOW);
            led_red.setState(false); // Atualiza o estado do switch no HA
        }

        // IMPORTANTE: Atualiza o último estado conhecido para o estado atual
        lastMotionState = currentMotionState;
    }

    // Lógica do sensor de temperatura e umidade (sem alterações)
    if (millis() - lastTime > PUBLISH_PERIOD) {
        lastTime = millis();
        TempAndHumidity data = dhtSensor.getTempAndHumidity();
        String temp = String(data.temperature, 1);
        String humi = String(data.humidity, 1);
        
        if (!isnan(data.temperature) && !isnan(data.humidity)) {
            Serial.println("Publishing DHT22 sensor data:");
            Serial.println("Temperature: " + temp + "°C");
            Serial.println("Humidity: " + humi + "%");
            Serial.println("---");
            dhtSensorTemp.setValue(temp.c_str());
            dhtSensorHumi.setValue(humi.c_str());
        }
    }
}