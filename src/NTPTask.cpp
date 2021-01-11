#include "NTPTask.h"

NTPTask::NTPTask() {
  init_client();
}

NTPTask::NTPTask(uint32_t update_interval) {
  init_client();
  _update_interval = update_interval;
}

void NTPTask::init_client() {
  WiFiUDP wifi_udp;
  _time_client = new NTPClient(wifi_udp);
}

bool NTPTask::_handleOnce(void* pvParameter) {
  NTPTask* task = (NTPTask *) pvParameter;
  if (task->_ntp_last_update == 0 || 
      task->_current_secs - task->_ntp_last_update > task->_update_interval) {
    Debug.println(F("NTP updating time..."));
    _updating = true;
    while (!task->_time_client->update()) {
      Debug.println(F("NTP forcing update..."));
      task->_time_client->forceUpdate();
    }
    task->_current_secs = task->_time_client->getEpochTime();
    task->_ntp_last_update = task->_current_secs;
    setTime(task->_current_secs);
    Debug.printf("NTP client updated, current epoch: %lu\n", (long unsigned int) task->_current_secs);
    _updating = false;
  }
  return true;
}

void NTPTask::_handle(void* pvParameter) {
  for (;;) {
    _handleOnce(pvParameter);
     vTaskDelay(60 * 1000 / portTICK_PERIOD_MS); // 60secs
  }
}

void NTPTask::begin() {
  begin(false);
}

void NTPTask::begin(bool update) {
#ifdef DEBUG_ENABLE_SERIAL
  bool serial = true;
#else
  bool serial = false;
#endif
#ifdef DEBUG_ENABLE_TELNET
  bool telnet = true;
#else
  bool telnet = false;
#endif
  Debug.begin(serial, telnet);
    
  _time_client->setTimeOffset(DEFAULT_TIME_OFFSET);
  _time_client->begin();
#if defined(ESP32_RTOS) && defined(ESP32)
  // if enabled, start RTOS task now
  Debug.println("Creating RTOS NTP task...");
  _task_create_result = xTaskCreate(
    _handle,                 /* Task function */
    "NTP_TASK_HANDLE",       /* String with name of task */
    10000,                   /* Stack size in bytes */
    (void*) this,            /* Parameter passed as input of the task */
    1,                       /* Priority of the task */
    &_task_handle);          /* Task handle */
#endif
  _task_setup = true;
  if (update) {
    _handleOnce((void*) this);
  }
}

bool NTPTask::update() {
  if (!_task_setup) begin();
#ifndef ESP32_RTOS
  return _handleOnce((void*) this); 
#endif  
}

//   if ((millis() - _lastUpdate >= _updateInterval)     // Update after _updateInterval
//     || _lastUpdate == 0) {                                // Update if there was no update yet.
//     if (!_udpSetup) begin();                         // setup the UDP client if needed
//     return forceUpdate();
//   }
//   return true;
// }

void NTPTask::end() {
  if (_task_setup) {
    _time_client->end();
    if (_task_create_result == pdPASS) {
      vTaskDelete(_task_handle);
      _task_create_result = pdFALSE;
    }
    _task_setup = false;
  }
}

void NTPTask::set_update_interval(uint32_t update_interval) {
  _update_interval = update_interval;
}

bool NTPTask::is_updating() {
  return _updating;
}

uint64_t NTPTask::get_time_epoch() {
  if (_task_setup) {
    return _time_client->getEpochTime();
  } else {
    return 0;
  }
}