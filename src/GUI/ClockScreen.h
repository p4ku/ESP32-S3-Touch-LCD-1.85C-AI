#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Create the clock screen (HH:MM:SS + date)
void GUI_CreateClockScreen();
void GUI_UpdateClockScreen(const struct tm& rtcTime);

#ifdef __cplusplus
}
#endif
