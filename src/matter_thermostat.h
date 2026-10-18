#pragma once
// EXPERIMENTAL: requires Arduino-ESP32 3.x with Matter enabled in its ESP-IDF build.
// Default PlatformIO environment does NOT enable this header.
#if defined(ENABLE_28LCD_MATTER) && ENABLE_28LCD_MATTER
#include <Matter.h>
static MatterThermostat matterThermostat;
static bool matterStarted = false;
static float matterPublishedTemp = NAN;
static float matterPublishedTarget = NAN;
static bool matterPublishedEnabled = false;
static uint32_t matterLastSync = 0;

static void matterThermostatBegin() {
  // Heating-only: UI AUTO maps to HEAT, OFF maps to OFF.
  if (!matterThermostat.begin(MatterThermostat::THERMOSTAT_SEQ_OP_HEATING,
                               MatterThermostat::THERMOSTAT_AUTO_MODE_DISABLED)) {
    Serial.println("[MATTER] Cannot create Thermostat endpoint");return;
  }
  if (!Matter.begin()) {
    Serial.println("[MATTER] Matter stack initialization failed");return;
  }
  matterStarted=true;
  matterThermostat.setHeatingSetpoint(setPoint);
  matterThermostat.setMode(heatingEnabled ? MatterThermostat::THERMOSTAT_MODE_HEAT :
                                            MatterThermostat::THERMOSTAT_MODE_OFF);
  if (temperatureValid && isfinite(roomTemp)) matterThermostat.setLocalTemperature(roomTemp);
  matterPublishedTarget=setPoint;
  matterPublishedEnabled=heatingEnabled;
  matterPublishedTemp=roomTemp;
  Serial.println("[MATTER] Started; use the actual commissioning payload from the firmware, not a guessed 11-digit PIN");
}

static void matterThermostatService() {
  if (!matterStarted || millis()-matterLastSync<1000) return;
  matterLastSync=millis();
  // Matter writes -> local control and NVS. LVGL widgets update without events.
  const auto mode=matterThermostat.getMode();
  const bool enabled=mode!=MatterThermostat::THERMOSTAT_MODE_OFF;
  const float remoteTarget=(float)matterThermostat.getHeatingSetpoint();
  bool localChanged=false;
  if (enabled!=matterPublishedEnabled && enabled!=heatingEnabled) {
    heatingEnabled=enabled; thermostatPrefs.putBool("enabled",enabled);localChanged=true;
    if(heatSwitch){
      if(enabled)lv_obj_add_state(heatSwitch,LV_STATE_CHECKED);
      else lv_obj_clear_state(heatSwitch,LV_STATE_CHECKED);
    }
  }
  if (isfinite(remoteTarget) && remoteTarget>=15.0f && remoteTarget<=30.0f &&
      fabsf(remoteTarget-matterPublishedTarget)>0.05f && fabsf(remoteTarget-setPoint)>0.05f) {
    setPoint=remoteTarget;thermostatPrefs.putFloat("target",setPoint);localChanged=true;
    if(targetSlider)lv_slider_set_value(targetSlider,(int)roundf(setPoint*10),LV_ANIM_OFF);
  }
  if(localChanged) thermostatRefresh();
  // Local UI/Web writes -> Matter attribute.
  if(fabsf(setPoint-remoteTarget)>0.05f)matterThermostat.setHeatingSetpoint(setPoint);
  if(heatingEnabled!=enabled)matterThermostat.setMode(heatingEnabled ?
      MatterThermostat::THERMOSTAT_MODE_HEAT : MatterThermostat::THERMOSTAT_MODE_OFF);
  matterPublishedTarget=setPoint;
  matterPublishedEnabled=heatingEnabled;
  if(temperatureValid && isfinite(roomTemp) &&
     (!isfinite(matterPublishedTemp)||fabsf(roomTemp-matterPublishedTemp)>0.09f)){
    matterThermostat.setLocalTemperature(roomTemp);
    matterPublishedTemp=roomTemp;
  }
}
#else
static void matterThermostatBegin() {}
static void matterThermostatService() {}
#endif
