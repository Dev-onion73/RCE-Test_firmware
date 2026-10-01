#ifndef RADIO_H
#define RADIO_H

namespace Radio {
    void begin();
    void service();
    void feature();

    bool isConfigured();
    bool isEnabled();
    bool isDetected();
    bool isReady();

    void setEnabled(bool enabled);
}

#endif