#ifndef ARCANUM_GAME_LIGHTING_OPTIONS_H_
#define ARCANUM_GAME_LIGHTING_OPTIONS_H_

static inline int lighting_reduction_percent(int value)
{
    return value == 25 || value == 50 ? value : 0;
}

// Reduce the distance from full light, rather than multiplying brightness.
// This lifts dark colors while leaving fully lit colors unchanged.
static inline int lighting_adjust_channel(int channel, int value)
{
    int percent = lighting_reduction_percent(value);
    return channel + ((255 - channel) * percent + 50) / 100;
}

#endif /* ARCANUM_GAME_LIGHTING_OPTIONS_H_ */
