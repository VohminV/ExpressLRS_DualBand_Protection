#pragma once

#include "targets.h"
#include "random.h"
//#include "SX12xxDriverCommon.h"

#if defined(RADIO_SX127X)
#define FreqCorrectionMax ((int32_t)(100000/FREQ_STEP))
#elif defined(RADIO_LR1121)
#define FreqCorrectionMax ((int32_t)(100000/FREQ_STEP)) // TODO - This needs checking !!!
#elif defined(RADIO_SX128X)
#define FreqCorrectionMax ((int32_t)(200000/FREQ_STEP))
#endif
#define FreqCorrectionMin (-FreqCorrectionMax)

#if defined(RADIO_LR1121)
#define FREQ_HZ_TO_REG_VAL(freq) (freq)
#define FREQ_SPREAD_SCALE 1
#else
#define FREQ_HZ_TO_REG_VAL(freq) ((uint32_t)((double)freq/(double)FREQ_STEP))
#define FREQ_SPREAD_SCALE 256
#endif

#define FHSS_SEQUENCE_LEN 256

typedef struct {
    const char  *domain;
    uint32_t    freq_start;
    uint32_t    freq_stop;
    uint32_t    freq_count;
} fhss_config_t;

typedef uint8_t SX12XX_Radio_Number;
enum
{
    SX12XX_Radio_One    = 0b00000001,     // Bit mask for radio 1
    SX12XX_Radio_Two    = 0b00000010,     // Bit mask for radio 2
};


extern volatile uint8_t FHSSptr;
extern int32_t FreqCorrection;      // Only used for the SX1276
extern int32_t FreqCorrection2;    // Only used for the SX1276

// Primary Band
extern uint16_t primaryBandCount;
extern uint32_t freq_spread;
extern uint32_t freq_spread2;
extern uint8_t FHSSsequence[];
extern uint8_t FHSSsequence2[];
extern uint_fast8_t sync_channel;
extern uint_fast8_t sync_channel2;
extern const fhss_config_t *FHSSconfig1;
extern const fhss_config_t *FHSSconfig2;

// DualBand Variables
extern bool FHSSusePrimaryFreqBand;
extern bool FHSSuseDualBand;
extern uint16_t secondaryBandCount;
extern uint32_t freq_spread_DualBand;
extern uint8_t FHSSsequence_DualBand[];
extern uint_fast8_t sync_channel_DualBand;
extern const fhss_config_t *FHSSconfig1DualBand;

// create and randomise an FHSS sequence
void FHSSrandomiseFHSSsequence(uint32_t seed, SX12XX_Radio_Number radioNumber);
void FHSSrandomiseFHSSsequenceBuild(uint32_t seed, uint32_t freqCount, uint_fast8_t sync_channel, uint8_t *sequence);

static inline uint32_t FHSSgetMinimumFreq(void)
{
    return FHSSconfig1->freq_start;
}

static inline uint32_t FHSSgetMaximumFreq(void)
{
    return FHSSconfig1->freq_stop;
}

// The number of frequencies for this regulatory domain
static inline uint32_t FHSSgetChannelCount(SX12XX_Radio_Number radioNumber)
{/*
    if (FHSSusePrimaryFreqBand)
    {
        return FHSSconfig1->freq_count;
    }
    else
    {
        return FHSSconfig1DualBand->freq_count;
    }*/
	if (radioNumber == SX12XX_Radio_One)
    {
        return FHSSconfig1->freq_count;
    }
    else if (radioNumber == SX12XX_Radio_Two)
    {
        return FHSSconfig2->freq_count;
    }

    return 0;  // Некорректное значение радио
}

// get the number of entries in the FHSS sequence
static inline uint16_t FHSSgetSequenceCount()
{
    if (FHSSuseDualBand) // Use the smaller of the 2 bands as not to go beyond the max index for each sequence.
    {
        if (primaryBandCount < secondaryBandCount)
        {
            return primaryBandCount;
        }
        else
        {
            return secondaryBandCount;
        }
    }

    if (FHSSusePrimaryFreqBand)
    {
        return primaryBandCount;
    }
    else
    {
        return secondaryBandCount;
    }
}

// get the initial frequency, which is also the sync channel
static inline uint32_t FHSSgetInitialFreq(SX12XX_Radio_Number radioNumber)
{
    if (radioNumber == SX12XX_Radio_One)
    {
       return FHSSconfig1->freq_start + 
                   (sync_channel * freq_spread / FREQ_SPREAD_SCALE) - FreqCorrection;
    }
    else if (radioNumber == SX12XX_Radio_Two)
    {
        return FHSSconfig2->freq_start + 
               (sync_channel2 * freq_spread2 / FREQ_SPREAD_SCALE) - FreqCorrection2;
    }

    return 0;
}


// Get the current sequence pointer
static inline uint8_t FHSSgetCurrIndex()
{
    return FHSSptr;
}

// Is the current frequency the sync frequency
static inline uint8_t FHSSonSyncChannel(SX12XX_Radio_Number radioNumber)
{
	/*
    if (FHSSusePrimaryFreqBand)
    {
        return FHSSsequence[FHSSptr] == sync_channel;
    }
    else
    {
        return FHSSsequence_DualBand[FHSSptr] == sync_channel_DualBand;
    }*/
	if (radioNumber == SX12XX_Radio_One)
    {
        return FHSSsequence[FHSSptr] == sync_channel;
    }
    else if (radioNumber == SX12XX_Radio_Two)
    {
        return FHSSsequence2[FHSSptr] == sync_channel2;
    }
    return false;
}

// Set the sequence pointer, used by RX on SYNC
static inline void FHSSsetCurrIndex(const uint8_t value)
{
    FHSSptr = value % FHSSgetSequenceCount();
}

// Advance the pointer to the next hop and return the frequency of that channel
static inline uint32_t FHSSgetNextFreq(SX12XX_Radio_Number radioNumber)
{/*
    FHSSptr = (FHSSptr + 1) % FHSSgetSequenceCount();

    if (FHSSusePrimaryFreqBand)
    {
        return FHSSconfig1->freq_start + (freq_spread * FHSSsequence[FHSSptr] / FREQ_SPREAD_SCALE) - FreqCorrection;
    }
    else
    {
        return FHSSconfig1DualBand->freq_start + (freq_spread_DualBand * FHSSsequence_DualBand[FHSSptr] / FREQ_SPREAD_SCALE);
    }*/
	FHSSptr = (FHSSptr + 1) % FHSSgetSequenceCount();

    if (radioNumber == SX12XX_Radio_One)
    {
        return FHSSconfig1->freq_start + 
               (freq_spread * FHSSsequence[FHSSptr] / FREQ_SPREAD_SCALE) - FreqCorrection;
    }
    else if (radioNumber == SX12XX_Radio_Two)
    {
        return FHSSconfig2->freq_start + 
               (freq_spread2 * FHSSsequence2[FHSSptr] / FREQ_SPREAD_SCALE) - FreqCorrection2;
    }

    return 0; // Некорректное значение радио
}

static inline const char *FHSSgetRegulatoryDomain()
{
        return FHSSconfig1->domain;
}

static inline const char *FHSSgetRegulatoryDomain2()
{
        return FHSSconfig2->domain;
}


// Get frequency offset by half of the domain frequency range
static inline uint32_t FHSSGeminiFreq(uint8_t FHSSsequenceIdx)
{
    uint32_t freq;
    uint32_t numfhss = FHSSgetChannelCount(SX12XX_Radio_Two);
    uint8_t offSetIdx = (FHSSsequenceIdx + (numfhss / 2)) % numfhss; 

    if (FHSSusePrimaryFreqBand)
    {
        freq = FHSSconfig1->freq_start + (freq_spread * offSetIdx / FREQ_SPREAD_SCALE) - FreqCorrection2;
    }
    else
    {
        freq = FHSSconfig1DualBand->freq_start + (freq_spread_DualBand * offSetIdx / FREQ_SPREAD_SCALE);
    }

    return freq;
}

static inline uint32_t FHSSgetGeminiFreq()
{
    if (FHSSuseDualBand)
    {
        // When using Dual Band there is no need to calculate an offset frequency. Unlike Gemini with 2 frequencies in the same band.
        return FHSSconfig1DualBand->freq_start + (FHSSsequence_DualBand[FHSSptr] * freq_spread_DualBand / FREQ_SPREAD_SCALE);
    }
    else
    {
        if (FHSSusePrimaryFreqBand)
        {
            return FHSSGeminiFreq(FHSSsequence[FHSSgetCurrIndex()]);
        }
        else
        {
            return FHSSGeminiFreq(FHSSsequence_DualBand[FHSSgetCurrIndex()]);
        }
    }
}

static inline uint32_t FHSSgetInitialGeminiFreq()
{
    if (FHSSuseDualBand)
    {
        return FHSSconfig1DualBand->freq_start + (sync_channel_DualBand * freq_spread_DualBand / FREQ_SPREAD_SCALE);
    }
    else
    {
        if (FHSSusePrimaryFreqBand)
        {
            return FHSSGeminiFreq(sync_channel);
        }
        else
        {
            return FHSSGeminiFreq(sync_channel_DualBand);
        }
    }
}
