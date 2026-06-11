#include <Arduino.h>
#include <Wire.h>

#include <TinyGPS++.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <Adafruit_BMP280.h>

#include <U8G2lib.h>

// =====================================================
// I2C ESP32
// =====================================================

#define SDA_PIN 21
#define SCL_PIN 22

// =====================================================
// GPS
// =====================================================

TinyGPSPlus gps;

HardwareSerial gpsSerial(2);

#define GPS_RX_PIN 16
#define GPS_TX_PIN 17

#define GPS_BAUD 9600

// =====================================================
// MPU6050
// =====================================================

Adafruit_MPU6050 mpu;

// =====================================================
// BMP280
// =====================================================

Adafruit_BMP280 bmp;

bool bmpOK = false;

// Presion a nivel del mar
#define SEA_LEVEL_HPA 1013.25

// Offset de calibracion
#define OFFSET_CALIBRACION 4.0

// =====================================================
// OLED SH1106
// =====================================================

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R0,
    U8X8_PIN_NONE
);

// =====================================================
// TIEMPOS
// =====================================================

unsigned long previousMillis = 0;

const unsigned long interval = 2000;

unsigned long startupMillis = 0;

bool mensajeInicioMostrado = false;

// =====================================================
// OLED PAGINAS
// =====================================================

unsigned long oledMillis = 0;

const unsigned long oledInterval = 4000;

int pantallaActual = 0;

// =====================================================
// VARIABLES MPU6050
// =====================================================

float ax, ay, az;

float gx, gy, gz;

// =====================================================
// VARIABLES BMP280
// =====================================================

float temperatura = 0;

float temperaturaCalibrada = 0;

float presion = 0;

float altitud = 0;

// =====================================================

void mostrarMensaje(const char *mensaje) {

    u8g2.clearBuffer();

    u8g2.setFont(
        u8g2_font_6x10_tr
    );

    u8g2.drawStr(
        0,
        20,
        mensaje
    );

    u8g2.sendBuffer();
}

// =====================================================

void setup() {

    Serial.begin(115200);

    // ================================================
    // I2C
    // ================================================

    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );

    Wire.setClock(100000);

    // ================================================
    // OLED
    // ================================================

    u8g2.begin();

    mostrarMensaje(
        "Iniciando..."
    );

    startupMillis = millis();

    // ================================================
    // GPS
    // ================================================

    gpsSerial.begin(
        GPS_BAUD,
        SERIAL_8N1,
        GPS_RX_PIN,
        GPS_TX_PIN
    );

    Serial.println();
    Serial.println(
        "GPS iniciado"
    );

    // ================================================
    // MPU6050
    // ================================================

    if (!mpu.begin()) {

        Serial.println(
            "No se encontro MPU6050"
        );

        mostrarMensaje(
            "Error MPU6050"
        );

    } else {

        Serial.println(
            "MPU6050 OK"
        );

        mpu.setAccelerometerRange(
            MPU6050_RANGE_8_G
        );

        mpu.setGyroRange(
            MPU6050_RANGE_500_DEG
        );

        mpu.setFilterBandwidth(
            MPU6050_BAND_21_HZ
        );
    }

    // ================================================
    // BMP280
    // ================================================

    if (bmp.begin(0x76)) {

        bmpOK = true;

        Serial.println(
            "BMP280 encontrado en 0x76"
        );

    } else if (bmp.begin(0x77)) {

        bmpOK = true;

        Serial.println(
            "BMP280 encontrado en 0x77"
        );

    } else {

        Serial.println(
            "BMP280 NO detectado"
        );
    }
}

// =====================================================

void loop() {

    // ================================================
    // MENSAJE INICIAL SIN DELAY
    // ================================================

    if (!mensajeInicioMostrado &&
        millis() - startupMillis > 1500) {

        mostrarMensaje(
            "Sistema listo"
        );

        mensajeInicioMostrado = true;
    }

    // ================================================
    // LEER GPS SIEMPRE
    // ================================================

    while (gpsSerial.available() > 0) {

        char c = gpsSerial.read();

        gps.encode(c);
    }

    // ================================================
    // VERIFICAR GPS
    // ================================================

    if (millis() > 5000 &&
        gps.charsProcessed() < 10) {

        Serial.println(
            "No llegan datos GPS"
        );
    }

    // ================================================
    // TIMER SENSORES
    // ================================================

    unsigned long currentMillis =
        millis();

    if (currentMillis - previousMillis >= interval) {

        previousMillis = currentMillis;

        // ============================================
        // MPU6050
        // ============================================

        sensors_event_t aceleracion;
        sensors_event_t giro;
        sensors_event_t temperaturaMPU;

        mpu.getEvent(
            &aceleracion,
            &giro,
            &temperaturaMPU
        );

        ax =
            aceleracion.acceleration.x;

        ay =
            aceleracion.acceleration.y;

        az =
            -aceleracion.acceleration.z;

        gx =
            giro.gyro.z * 180.0 / PI;

        gy =
            giro.gyro.y * 180.0 / PI;

        gz =
            giro.gyro.x * 180.0 / PI;

        // ============================================
        // BMP280
        // ============================================

        if (bmpOK) {

            temperatura =
                bmp.readTemperature();

            if (
                temperatura > 100 ||
                temperatura < -40
            ) {

                Serial.println(
                    "Temperatura BMP280 invalida"
                );

                temperatura = 0;
            }

            temperaturaCalibrada =
                temperatura - OFFSET_CALIBRACION;

            presion =
                bmp.readPressure() / 100.0;

            altitud =
                bmp.readAltitude(
                    SEA_LEVEL_HPA
                );
        }

        // ============================================
        // SERIAL
        // ============================================

        Serial.println();
        Serial.println(
            "========== DATOS =========="
        );

        // ============================================
        // MPU6050
        // ============================================

        Serial.println(
            "----- MPU6050 -----"
        );

        Serial.print("AX: ");
        Serial.println(ax);

        Serial.print("AY: ");
        Serial.println(ay);

        Serial.print("AZ: ");
        Serial.println(az);

        Serial.print("GX: ");
        Serial.println(gx);

        Serial.print("GY: ");
        Serial.println(gy);

        Serial.print("GZ: ");
        Serial.println(gz);

        // ============================================
        // BMP280
        // ============================================

        if (bmpOK) {

            Serial.println(
                "----- BMP280 -----"
            );

            Serial.print(
                "Temperatura: "
            );

            Serial.println(
                temperaturaCalibrada
            );

            Serial.print(
                "Presion: "
            );

            Serial.println(
                presion
            );

            Serial.print(
                "Altitud: "
            );

            Serial.println(
                altitud
            );

        } else {

            Serial.println(
                "BMP280 NO disponible"
            );
        }

        // ============================================
        // GPS
        // ============================================

        Serial.println(
            "----- GPS -----"
        );

        Serial.print(
            "Caracteres: "
        );

        Serial.println(
            gps.charsProcessed()
        );

        Serial.print(
            "Satelites: "
        );

        if (gps.satellites.isValid()) {

            Serial.println(
                gps.satellites.value()
            );

        } else {

            Serial.println(
                "No disponible"
            );
        }

        if (gps.location.isValid()) {

            Serial.print(
                "Latitud: "
            );

            Serial.println(
                gps.location.lat(),
                6
            );

            Serial.print(
                "Longitud: "
            );

            Serial.println(
                gps.location.lng(),
                6
            );

        } else {

            Serial.println(
                "GPS sin coordenadas"
            );
        }
    }

    // ================================================
    // CAMBIO DE PANTALLA OLED
    // ================================================

    if (millis() - oledMillis >= oledInterval) {

        oledMillis = millis();

        pantallaActual++;

        if (pantallaActual > 2) {

            pantallaActual = 0;
        }
    }

    // ================================================
    // OLED POR PAGINAS
    // ================================================

    char linea[32];

    u8g2.clearBuffer();

    u8g2.setFont(
        u8g2_font_6x10_tr
    );

    // =====================================================
    // PANTALLA BMP280
    // =====================================================

    if (pantallaActual == 0) {

        u8g2.drawStr(
            0,
            10,
            "BMP280"
        );

        if (bmpOK) {

            snprintf(
                linea,
                sizeof(linea),
                "TEMP: %.2f C",
                temperaturaCalibrada
            );

            u8g2.drawStr(
                0,
                28,
                linea
            );

            snprintf(
                linea,
                sizeof(linea),
                "PRES: %.1f hPa",
                presion
            );

            u8g2.drawStr(
                0,
                42,
                linea
            );

            snprintf(
                linea,
                sizeof(linea),
                "ALT: %.1f m",
                altitud
            );

            u8g2.drawStr(
                0,
                56,
                linea
            );

        } else {

            u8g2.drawStr(
                0,
                30,
                "BMP280 NO DETECTADO"
            );
        }
    }

    // =====================================================
    // PANTALLA MPU6050
    // =====================================================

    else if (pantallaActual == 1) {

        u8g2.drawStr(
            0,
            10,
            "MPU6050"
        );

        snprintf(
            linea,
            sizeof(linea),
            "AX: %.2f",
            ax
        );

        u8g2.drawStr(
            0,
            24,
            linea
        );

        snprintf(
            linea,
            sizeof(linea),
            "AY: %.2f",
            ay
        );

        u8g2.drawStr(
            0,
            36,
            linea
        );

        snprintf(
            linea,
            sizeof(linea),
            "AZ: %.2f",
            az
        );

        u8g2.drawStr(
            0,
            48,
            linea
        );

        snprintf(
            linea,
            sizeof(linea),
            "GX: %.2f",
            gx
        );

        u8g2.drawStr(
            0,
            60,
            linea
        );
    }

    // =====================================================
    // PANTALLA GPS
    // =====================================================

    else if (pantallaActual == 2) {

        u8g2.drawStr(
            0,
            10,
            "GPS"
        );

        snprintf(
            linea,
            sizeof(linea),
            "SAT: %d",
            gps.satellites.isValid()
                ? gps.satellites.value()
                : 0
        );

        u8g2.drawStr(
            0,
            24,
            linea
        );

        snprintf(
            linea,
            sizeof(linea),
            "CH: %lu",
            gps.charsProcessed()
        );

        u8g2.drawStr(
            0,
            36,
            linea
        );

        if (gps.location.isValid()) {

            snprintf(
                linea,
                sizeof(linea),
                "LAT: %.4f",
                gps.location.lat()
            );

            u8g2.drawStr(
                0,
                50,
                linea
            );

            snprintf(
                linea,
                sizeof(linea),
                "LON: %.4f",
                gps.location.lng()
            );

            u8g2.drawStr(
                0,
                62,
                linea
            );

        } else {

            u8g2.drawStr(
                0,
                50,
                "BUSCANDO SENAL"
            );
        }
    }

    u8g2.sendBuffer();
}
