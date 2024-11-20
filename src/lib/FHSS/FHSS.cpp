#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>

#if defined(RADIO_SX127X) || defined(RADIO_LR1121)

#if defined(RADIO_LR1121)
#include "LR1121Driver.h"
#else
#include "SX127xDriver.h"
#endif


const fhss_config_t domains[] = {
    {"AU915",  FREQ_HZ_TO_REG_VAL(915500000), FREQ_HZ_TO_REG_VAL(926900000), 20},
};

const fhss_config_t domains2[] = {
    {"EU433",  FREQ_HZ_TO_REG_VAL(433100000), FREQ_HZ_TO_REG_VAL(434450000), 3},
};

#if defined(RADIO_LR1121)
const fhss_config_t domainsDualBand[] = {
    {"ISM2G4", FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80}
};
#endif

#elif defined(RADIO_SX128X)
#include "SX1280Driver.h"

const fhss_config_t domains[] = {
    {    
    #if defined(Regulatory_Domain_EU_CE_2400)
        "CE_LBT",
    #elif defined(Regulatory_Domain_ISM_2400)
        "ISM2G4",
    #endif
    FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80}
};
#endif

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
const fhss_config_t *FHSSconfig1;
const fhss_config_t *FHSSconfig2;
const fhss_config_t *FHSSconfigRadio1DualBand;

// Actual sequence of hops as indexes into the frequency list
uint8_t FHSSsequence[FHSS_SEQUENCE_LEN];
uint8_t FHSSsequence2[FHSS_SEQUENCE_LEN];
uint8_t FHSSsequence_DualBand[FHSS_SEQUENCE_LEN];

// Which entry in the sequence we currently are on
uint8_t volatile FHSSptr;

// Channel for sync packets and initial connection establishment
uint_fast8_t sync_channel;
uint_fast8_t sync_channel2;
uint_fast8_t sync_channel_DualBand;

// Offset from the predefined frequency determined by AFC on Team900 (register units)
int32_t FreqCorrection;
int32_t FreqCorrection2;

// Frequency hop separation
uint32_t freq_spread;
uint32_t freq_spread2;
uint32_t freq_spread_DualBand;

// Variable for Dual Band radios
bool FHSSusePrimaryFreqBand = true;
bool FHSSuseDualBand = false;

uint16_t primaryBandCount;
uint16_t secondaryBandCount;

void FHSSrandomiseFHSSsequence(const uint32_t seed, SX12XX_Radio_Number radioNumber)
{
	if (radioNumber == SX12XX_Radio_One)
    {
		FHSSconfig1 = &domains[firmwareOptions.domain];
		sync_channel = (FHSSconfig1->freq_count / 2) + 1;
		freq_spread = (FHSSconfig1->freq_stop - FHSSconfig1->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfig1->freq_count - 1);
		primaryBandCount = (FHSS_SEQUENCE_LEN / FHSSconfig1->freq_count) * FHSSconfig1->freq_count;

		FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfig1->freq_count, sync_channel, FHSSsequence);

     }
    else if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
		if (radioNumber == SX12XX_Radio_Two)
		{
			FHSSconfig2 = &domains2[firmwareOptions.domain2];
			sync_channel2 = (FHSSconfig2->freq_count / 2) + 1;
			freq_spread2 = (FHSSconfig2->freq_stop - FHSSconfig2->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfig2->freq_count - 1);
			
			FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfig2->freq_count, sync_channel2, FHSSsequence2);
		}
	}

#if defined(RADIO_LR1121)
    FHSSconfigRadio1DualBand = &domainsDualBand[0];
    sync_channel_DualBand = (FHSSconfigRadio1DualBand->freq_count / 2) + 1;
    freq_spread_DualBand = (FHSSconfigRadio1DualBand->freq_stop - FHSSconfigRadio1DualBand->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfigRadio1DualBand->freq_count - 1);
    secondaryBandCount = (FHSS_SEQUENCE_LEN / FHSSconfigRadio1DualBand->freq_count) * FHSSconfigRadio1DualBand->freq_count;

    DBGLN("Setting Dual Band %s Mode", FHSSconfigRadio1DualBand->domain);
    DBGLN("Number of FHSS frequencies = %u", FHSSconfigRadio1DualBand->freq_count);
    DBGLN("Sync channel Dual Band = %u", sync_channel_DualBand);

    FHSSusePrimaryFreqBand = false;
    FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfigRadio1DualBand->freq_count, sync_channel_DualBand, FHSSsequence_DualBand);
    FHSSusePrimaryFreqBand = true;
#endif
}

/**
Requirements:
1. 0 every n hops
2. No two repeated channels
3. Equal occurance of each (or as even as possible) of each channel
4. Pseudorandom

Approach:
  Fill the sequence array with the sync channel every FHSS_FREQ_CNT
  Iterate through the array, and for each block, swap each entry in it with
  another random entry, excluding the sync channel.

*/
void FHSSrandomiseFHSSsequenceBuild(const uint32_t seed, uint32_t freqCount, uint_fast8_t syncChannel, uint8_t *inSequence)
{
    // reset the pointer (otherwise the tests fail)
    FHSSptr = 0;
    rngSeed(seed);

    // initialize the sequence array
    for (uint16_t i = 0; i < FHSSgetSequenceCount(); i++)
    {
        if (i % freqCount == 0) {
            inSequence[i] = syncChannel;
        } else if (i % freqCount == syncChannel) {
            inSequence[i] = 0;
        } else {
            inSequence[i] = i % freqCount;
        }
    }

    for (uint16_t i = 0; i < FHSSgetSequenceCount(); i++)
    {
        // if it's not the sync channel
        if (i % freqCount != 0)
        {
            uint8_t offset = (i / freqCount) * freqCount;   // offset to start of current block
            uint8_t rand = rngN(freqCount - 1) + 1;         // random number between 1 and FHSS_FREQ_CNT

            // switch this entry and another random entry in the same block
            uint8_t temp = inSequence[i];
            inSequence[i] = inSequence[offset+rand];
            inSequence[offset+rand] = temp;
        }
    }

    // output FHSS sequence
    for (uint16_t i=0; i < FHSSgetSequenceCount(); i++)
    {
        DBG("%u ",inSequence[i]);
        if (i % 10 == 9)
            DBGCR;
    }
    DBGCR;
}

bool isDomain868()
{
    return strcmp(FHSSconfig1->domain, "EU868") == 0;
}
