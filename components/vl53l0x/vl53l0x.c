// vl53l0x.c
#include "vl53l0x.h"

// Registros importantes del VL53L0X
#define REG_IDENTIFICATION_MODEL_ID    0xC0
#define REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV    0x89
#define REG_MSRC_CONFIG_CONTROL        0x60
#define REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT  0x44
#define REG_SYSTEM_SEQUENCE_CONFIG     0x01
#define REG_DYNAMIC_SPAD_REF_EN_START_OFFSET  0x4F
#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD  0x4E
#define REG_GLOBAL_CONFIG_REF_EN_START_SELECT  0xB6
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO  0x0A
#define REG_GPIO_HV_MUX_ACTIVE_HIGH   0x84
#define REG_SYSTEM_INTERRUPT_CLEAR    0x0B
#define REG_RESULT_INTERRUPT_STATUS   0x13
#define REG_SYSRANGE_START            0x00
#define REG_RESULT_RANGE_STATUS       0x14
#define REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW  0x47
#define REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH  0x48

// Constantes
#define VL53L0X_EXPECTED_DEVICE_ID    0xEE
#define VL53L0X_I2C_TIMEOUT_MS        100

// Escritura/lectura I2C
static esp_err_t vl53l0x_write_reg(vl53l0x_sensor_t *sensor, uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(sensor->i2c_port, sensor->address, write_buf, 2, 
                                     sensor->timeout_ms / portTICK_PERIOD_MS);
}

static esp_err_t vl53l0x_write_reg16(vl53l0x_sensor_t *sensor, uint8_t reg, uint16_t data) {
    uint8_t write_buf[3] = {reg, (uint8_t)(data >> 8), (uint8_t)(data & 0xFF)};
    return i2c_master_write_to_device(sensor->i2c_port, sensor->address, write_buf, 3, 
                                     sensor->timeout_ms / portTICK_PERIOD_MS);
}

static esp_err_t vl53l0x_read_reg(vl53l0x_sensor_t *sensor, uint8_t reg, uint8_t *data) {
    return i2c_master_write_read_device(sensor->i2c_port, sensor->address, &reg, 1, 
                                       data, 1, sensor->timeout_ms / portTICK_PERIOD_MS);
}

static esp_err_t vl53l0x_read_reg16(vl53l0x_sensor_t *sensor, uint8_t reg, uint16_t *data) {
    uint8_t read_buf[2];
    esp_err_t ret = i2c_master_write_read_device(sensor->i2c_port, sensor->address, 
                                               &reg, 1, read_buf, 2, 
                                               sensor->timeout_ms / portTICK_PERIOD_MS);
    if (ret == ESP_OK) {
        *data = ((uint16_t)read_buf[0] << 8) | read_buf[1];
    }
    return ret;
}

bool vl53l0x_init(vl53l0x_sensor_t *sensor, i2c_port_t i2c_port, uint8_t address) {
    if (sensor == NULL) {
        ESP_LOGE("VL53L0X", "Sensor structure is NULL");
        return false;
    }
    
    // Inicializar estructura
    sensor->i2c_port = i2c_port;
    sensor->address = address;
    sensor->timeout_ms = VL53L0X_I2C_TIMEOUT_MS;
    sensor->calibration_factor = 1.0f;
    sensor->mode = VL53L0X_MODE_SINGLE;
    sensor->accuracy = VL53L0X_ACCURACY_BETTER;
    
    // Verificar ID del dispositivo
    uint8_t device_id;
    if (vl53l0x_read_reg(sensor, REG_IDENTIFICATION_MODEL_ID, &device_id) != ESP_OK) {
        ESP_LOGE("VL53L0X", "Error reading device ID");
        return false;
    }
    
    if (device_id != VL53L0X_EXPECTED_DEVICE_ID) {
        ESP_LOGE("VL53L0X", "Unexpected device ID: 0x%02X, expected: 0x%02X", 
                 device_id, VL53L0X_EXPECTED_DEVICE_ID);
        return false;
    }
    
    // Secuencia de inicialización según datasheet
    // Habilitar HV, seleccionar 2.8V para EXTSUP
    vl53l0x_write_reg(sensor, REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, 
                     (vl53l0x_read_reg(sensor, REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, &device_id) & 0xFE) | 0x01);
    
    // Configuración inicial de timing y calibración
    vl53l0x_set_accuracy(sensor, sensor->accuracy);
    
    // Configurar modo por defecto (medición única)
    vl53l0x_set_mode(sensor, sensor->mode);
    
    ESP_LOGI("VL53L0X", "Sensor initialized successfully");
    return true;
}

uint16_t vl53l0x_read_distance(vl53l0x_sensor_t *sensor) {
    if (sensor == NULL) {
        return 0;
    }
    
    esp_err_t ret;
    uint8_t status;
    uint16_t range_mm = 0;
    
    // En modo único, iniciar una medición
    if (sensor->mode == VL53L0X_MODE_SINGLE) {
        ret = vl53l0x_write_reg(sensor, REG_SYSRANGE_START, 0x01);
        if (ret != ESP_OK) {
            return 0;
        }
        
        // Esperar a que la medición se complete
        do {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            ret = vl53l0x_read_reg(sensor, REG_RESULT_INTERRUPT_STATUS, &status);
            if (ret != ESP_OK) {
                return 0;
            }
        } while ((status & 0x07) == 0);
        
        // Leer el resultado
        ret = vl53l0x_read_reg16(sensor, REG_RESULT_RANGE_STATUS + 10, &range_mm);
        if (ret != ESP_OK) {
            return 0;
        }
        
        // Limpiar la interrupción
        vl53l0x_write_reg(sensor, REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    } else {
        // En modo continuo o temporizado, simplemente leer el último valor
        ret = vl53l0x_read_reg16(sensor, REG_RESULT_RANGE_STATUS + 10, &range_mm);
        if (ret != ESP_OK) {
            return 0;
        }
    }
    
    // Aplicar factor de calibración
    return (uint16_t)((float)range_mm * sensor->calibration_factor);
}

bool vl53l0x_set_mode(vl53l0x_sensor_t *sensor, vl53l0x_mode_t mode) {
    if (sensor == NULL) {
        return false;
    }
    
    uint8_t system_mode;
    esp_err_t ret;
    
    // Detener cualquier medición en curso
    ret = vl53l0x_write_reg(sensor, REG_SYSRANGE_START, 0x00);
    if (ret != ESP_OK) {
        return false;
    }
    
    switch (mode) {
        case VL53L0X_MODE_SINGLE:
            // Modo por defecto, no necesita configuración adicional
            break;
            
        case VL53L0X_MODE_CONTINUOUS:
            // Iniciar modo continuo
            ret = vl53l0x_write_reg(sensor, REG_SYSRANGE_START, 0x02);
            if (ret != ESP_OK) {
                return false;
            }
            break;
            
        case VL53L0X_MODE_TIMED:
            // Configurar modo temporizado (por ejemplo, cada 100ms)
            // Este modo necesita configuración adicional según la aplicación
            ret = vl53l0x_write_reg(sensor, REG_SYSRANGE_START, 0x03);
            if (ret != ESP_OK) {
                return false;
            }
            break;
            
        default:
            return false;
    }
    
    sensor->mode = mode;
    return true;
}

bool vl53l0x_set_accuracy(vl53l0x_sensor_t *sensor, vl53l0x_accuracy_t accuracy) {
    if (sensor == NULL) {
        return false;
    }
    
    uint8_t timing_config;
    uint16_t timing_budget_us;
    
    // Configurar la precisión/velocidad
    switch (accuracy) {
        case VL53L0X_ACCURACY_GOOD:
            // Precisión estándar (~30ms por medición)
            timing_budget_us = 30000;
            vl53l0x_write_reg(sensor, REG_MSRC_CONFIG_CONTROL, 0x1D);
            break;
            
        case VL53L0X_ACCURACY_BETTER:
            // Mejor precisión (~70ms por medición)
            timing_budget_us = 70000;
            vl53l0x_write_reg(sensor, REG_MSRC_CONFIG_CONTROL, 0x1E);
            break;
            
        case VL53L0X_ACCURACY_BEST:
            // La mejor precisión (~200ms por medición)
            timing_budget_us = 200000;
            vl53l0x_write_reg(sensor, REG_MSRC_CONFIG_CONTROL, 0x1F);
            break;
            
        case VL53L0X_ACCURACY_FAST:
            // Precisión reducida, pero más rápido (~20ms)
            timing_budget_us = 20000;
            vl53l0x_write_reg(sensor, REG_MSRC_CONFIG_CONTROL, 0x1C);
            break;
            
        case VL53L0X_ACCURACY_FASTER:
            // Precisión muy reducida, muy rápido (~10ms)
            timing_budget_us = 10000;
            vl53l0x_write_reg(sensor, REG_MSRC_CONFIG_CONTROL, 0x18);
            break;
            
        default:
            return false;
    }
    
    // Establecer límite de tasa de retorno para mejorar la precisión
    // en función del tiempo de medición
    uint16_t return_limit;
    if (timing_budget_us > 50000) {
        return_limit = 0x0A00;  // ~0.25 MCPS
    } else {
        return_limit = 0x1400;  // ~0.5 MCPS
    }
    
    vl53l0x_write_reg16(sensor, REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, return_limit);
    
    // Configurar parámetros de fase de rango final
    vl53l0x_write_reg16(sensor, REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);
    vl53l0x_write_reg16(sensor, REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x78);
    
    sensor->accuracy = accuracy;
    return true;
}

void vl53l0x_set_calibration(vl53l0x_sensor_t *sensor, float factor) {
    if (sensor != NULL && factor > 0) {
        sensor->calibration_factor = factor;
    }
}

bool vl53l0x_wake_up(vl53l0x_sensor_t *sensor) {
    if (sensor == NULL) {
        return false;
    }
    
    // Escribir al registro de power para despertar el sensor
    return vl53l0x_write_reg(sensor, REG_SYSRANGE_START, 0x00) == ESP_OK;
}

bool vl53l0x_sleep(vl53l0x_sensor_t *sensor) {
    if (sensor == NULL) {
        return false;
    }
    
    // No hay un verdadero modo de sueño profundo en el VL53L0X,
    // pero podemos detener las mediciones activas
    return vl53l0x_write_reg(sensor, REG_SYSRANGE_START, 0x00) == ESP_OK;
}