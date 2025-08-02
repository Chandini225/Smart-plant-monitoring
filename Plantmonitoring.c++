#include <DHT.h>

// === CONFIGURATION ===
#define PLANT_TYPE "Fern"
#define MOISTURE_SENSOR_PIN 34
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define HISTORY_SIZE 20
#define LED_PIN 12

// === PLANT PROFILE ===
typedef struct {
    const char* name;
    float min_moisture;
    float max_moisture;
    float min_light;     // lux
    float max_light;
    float min_temp;      // °C
    float max_temp;
} PlantProfile;

PlantProfile plant_profiles[] = {
    {"Fern", 40.0, 60.0, 100.0, 500.0, 18.0, 24.0},
    {"Cactus", 5.0, 20.0, 1000.0, 5000.0, 15.0, 35.0}
};

// === SENSOR HISTORY ===
typedef struct {
    unsigned long timestamp;
    float moisture;
    float light;
    float temperature;
    uint8_t action_taken;
} SensorHistory;

SensorHistory history[HISTORY_SIZE];
int history_index = 0;

// === SENSOR OBJECTS ===
DHT dht(DHT_PIN, DHT_TYPE);

// === THRESHOLDS ===
float moisture_thresholds[] = {0.0, 30.0, 60.0}; // DRY, MODERATE, WET

enum MoistureLevel { DRY, MODERATE, WET };

// === UTILITY FUNCTIONS ===
PlantProfile* get_plant_profile(const char* name) {
    for (int i = 0; i < sizeof(plant_profiles)/sizeof(plant_profiles[0]); i++) {
        if (strcmp(plant_profiles[i].name, name) == 0) {
            return &plant_profiles[i];
        }
    }
    return NULL;
}

MoistureLevel classify_moisture(float value) {
    int low = 0;
    int high = sizeof(moisture_thresholds)/sizeof(moisture_thresholds[0]) - 1;
    int best = -1;

    while (low <= high) {
        int mid = (low + high) / 2;
        if (value >= moisture_thresholds[mid]) {
            best = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return (MoistureLevel)best;
}

void log_sensor_data(unsigned long timestamp, float moisture, float light, float temp, uint8_t action) {
    history[history_index].timestamp = timestamp;
    history[history_index].moisture = moisture;
    history[history_index].light = light;
    history[history_index].temperature = temp;
    history[history_index].action_taken = action;

    history_index = (history_index + 1) % HISTORY_SIZE;
}

int find_closest_history_index(unsigned long target_time) {
    int low = 0, high = HISTORY_SIZE - 1;
    int closest = 0;
    long min_diff = abs((long)(history[0].timestamp - target_time));

    while (low <= high) {
        int mid = (low + high) / 2;
        long diff = abs((long)(history[mid].timestamp - target_time));

        if (diff < min_diff) {
            min_diff = diff;
            closest = mid;
        }

        if (history[mid].timestamp < target_time) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return closest;
}

void make_decision(PlantProfile* profile, float moisture, float light, float temp, unsigned long current_time) {
    MoistureLevel m_level = classify_moisture(moisture);
    int similar_index = find_closest_history_index(current_time - 86400000); // 24 hours ago
    SensorHistory* past = &history[similar_index];

    Serial.print("Current Moisture: ");
    Serial.print(moisture);
    Serial.print(" (");
    Serial.print(m_level == DRY ? "Dry" : m_level == MODERATE ? "Moderate" : "Wet");
    Serial.println(")");

    if (moisture < profile->min_moisture) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Moisture too low. Consider watering.");
    } else if (moisture > profile->max_moisture) {
        digitalWrite(LED_PIN, LOW);
        Serial.println("Moisture too high. Reduce watering.");
    } else {
        digitalWrite(LED_PIN, LOW);
    }

    if (past->moisture < moisture && past->action_taken == 0) {
        Serial.println("Past data suggests current moisture increase is normal.");
    } else if (past->moisture > moisture && past->action_taken == 0) {
        Serial.println("Unusual moisture drop detected.");
    }
}

// === MAIN LOOP ===
void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.begin(115200);
    dht.begin();

    // Optional: I2C not used anymore
    // Wire.begin();  // Can be removed if not used by other sensors
}

void loop() {
    static unsigned long last_time = 0;
    static unsigned long current_time = 0;

    if (millis() - last_time > 10000) { // every 10 seconds
        last_time = millis();
        current_time += 10;

        float moisture_raw = analogRead(MOISTURE_SENSOR_PIN);
        float moisture_percent = map(moisture_raw, 0, 4095, 0, 100); // ESP32 uses 12-bit ADC

        float light_lux = random(100, 1000);  // Simulated light level
        float temperature = dht.readTemperature();

        PlantProfile* profile = get_plant_profile(PLANT_TYPE);

        log_sensor_data(current_time, moisture_percent, light_lux, temperature, 0); // no action yet

        Serial.print("Time: ");
        Serial.print(current_time);
        Serial.print("s | Moisture: ");
        Serial.print(moisture_percent);
        Serial.print("% | Light: ");
        Serial.print(light_lux);
        Serial.print(" lux | Temp: ");
        Serial.print(temperature);
        Serial.println(" C");

        make_decision(profile, moisture_percent, light_lux, temperature, current_time);
        Serial.println("----------------------------");
    }
}
