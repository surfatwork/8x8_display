/*
 * 8-Channel Spectrum Analyzer for Single 8x8 MAX7219 Display
 * Frequencies: 63Hz, 125Hz, 250Hz, 1KHz, 2KHz, 4KHz, 8KHz, 16KHz
 *
 * Based on original 32-channel version, modified for 8 frequency bands
 * Hardware: Arduino Uno/Nano + MAX7219 8x8 LED Matrix + Audio Input on A0
 */

#include <arduinoFFT.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define SAMPLES 128
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES  1          // Changed from 4 to 1 for single 8x8 module
#define CLK_PIN   13
#define DATA_PIN  11
#define CS_PIN    10
#define xres 8                  // Changed from 32 to 8 channels
#define yres 8


// Bit patterns for column display (0-8 LEDs lit)
int MY_ARRAY[] = {0, 1, 3, 7, 15, 31, 63, 127, 255};

double vReal[SAMPLES];
double vImag[SAMPLES];
float data_avgs[xres];
int yvalue;
int displaycolumn, displayvalue;
int peaks[xres];
float overallGain = 0.60;
long dcOffset = 508L;
unsigned long lastAudioTime = 0;
int waitforAudio = 5000;
const uint16_t peakDecayTime = 0;

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
arduinoFFT FFT = arduinoFFT();

// Frequency band definitions for 8 channels
// Based on ~38.5kHz sample rate, 64 samples = ~600Hz per bin
// Each band defined by start bin (inclusive) and end bin (exclusive)
const int bandStart[8] = {2, 3, 5, 8, 12, 18, 25, 32};
const int bandEnd[8]   = {3, 5, 8, 12, 18, 25, 32, 64};
// Columns roughly correspond to
// 1 | 300Hz
// 2 | 1025Hz
// 3 | 1800Hz
// 4 | 3000Hz
// 5 | 4000Hz
// 6 | 6000Hz
// 7 | 8000Hz
// 8 | 10000Hz

// | Column | FFT bins | Approx frequency range |
// | ------ | -------- | ---------------------- |
// | 1      | 1        | 308 Hz                 |
// | 2      | 2-3      | 616–924 Hz             |
// | 3      | 4-7      | 1.2–2.2 kHz            |
// | 4      | 8-11     | 2.5–3.4 kHz            |
// | 5      | 12-17    | 3.7–5.2 kHz            |
// | 6      | 18-24    | 5.5–7.4 kHz            |
// | 7      | 25-31    | 7.7–9.5 kHz            |
// | 8      | 32-63    | 9.8–19.4 kHz           |


// Sensitivity multiplier for each band (bass often needs boosting)
const float bandGain[8] =
{
    1.5,
    1.7,
    1.7,
    4.00,
    5.00,
    4.50,
    4.00,
    4.00
};

const uint8_t breatheTable[] =
{
 0,0,0,1,1,2,2,3,4,5,5,4,3,2,2,1,1,0,0,0
};

// const uint8_t breatheTable[] =
// {
//  0,0,0,1,1,2,2,3,4,5,6,7,
//  8,9,10,11,12,13,14,15,
//  15,14,13,12,11,10,9,8,
//  7,6,5,4,3,2,2,1,1,0
// };

const uint8_t breatheSteps =
    sizeof(breatheTable) / sizeof(breatheTable[0]);

uint8_t breatheIndex = 0;
uint8_t idleLevel = 0;

void setup() {
    Serial.begin(115200);
    // Configure ADC for fast sampling (~38.5kHz)
    ADCSRA = 0b11100101;      // Enable ADC, start conversion, auto-trigger, prescaler 32
    ADMUX = 0b01000000;       // ADC0 (A0), AREF, right-adjusted
    for (int i = 0; i < xres; i++)
        peaks[i] = 0;
    mx.begin();
    mx.control(MD_MAX72XX::INTENSITY, 1);
    mx.clear();
    mx.update();
    lastAudioTime = millis();
    // delay(1000);
    delay(50);
}

void loop() {
    // read overall gain

static unsigned long lastPotRead = 0;

if (millis() - lastPotRead > 100) {
    lastPotRead = millis();

    // Stop free-running mode
    ADCSRA = 0;

    // Select A1
    ADMUX = 0b01000001;

    // Enable ADC, prescaler 128
    ADCSRA = (1 << ADEN) |
    (1 << ADPS2) |
    (1 << ADPS1) |
    (1 << ADPS0);

    // Throw away first conversion
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));

    // Real conversion
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));

    int pot = ADC;

    overallGain = 1.5 * pow(pot / 1023.0, 2.0);

    // Restore FFT ADC configuration
    ADMUX  = 0b01000000;   // A0
    ADCSRA = 0b11100101;   // free-running mode

    // for pulsing
    // bool audioPresent = false;

    // for (int i = 0; i < xres; i++) {
    //     if (data_avgs[i] > 5) {
    //         audioPresent = true;
    //         break;
    //     }
    // }

    // if (audioPresent)
    // {
    //     lastAudioTime = millis();
    // }
}

static unsigned long lastPrint = 0;

// static unsigned long lastPrint = 0;
// Sample audio data
for (int i = 0; i < SAMPLES; i++)
{
    while (!(ADCSRA & 0x10));

    ADCSRA = 0b11110101;

    int raw = ADC;

    // slow adaptive DC tracking
    dcOffset += ((long)raw - dcOffset) >> 6;

    int value = raw - dcOffset;

    vReal[i] = value / 8.0;
    vImag[i] = 0;
}


// Perform FFT
// FFT.DCRemoval(vReal, SAMPLES);
FFT.DCRemoval(vReal, SAMPLES);
FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);

// Calculate magnitude for each of the 8 frequency bands
for (int band = 0; band < xres; band++) {

    float maxVal = 0;

    // Find strongest FFT bin in this band
    for (int bin = bandStart[band]; bin < bandEnd[band]; bin++) {
        if (vReal[bin] > maxVal)
            maxVal = vReal[bin];
    }

    // Apply band-specific gain
    // data_avgs[band] = maxVal * bandGain[band];
maxVal -= 6;

if (maxVal < 0)
maxVal = 0;

data_avgs[band] = maxVal * bandGain[band] * overallGain;
}

bool audioPresent = false;
// commented out below for trying out pulse test
// for (int i = 0; i < xres; i++) {
//     if (data_avgs[i] > 5) {
//         audioPresent = true;
//         break;
//     }
// }
//  until here commented out

float totalEnergy = 0;

for (int i = 0; i < xres; i++) {
    totalEnergy += data_avgs[i];
}

audioPresent = (totalEnergy > 30);
if (audioPresent)
{
    lastAudioTime = millis();
}

// pulse when no music
if (!audioPresent)
{
//         // for debug pulse not starting
//         Serial.print("Audio=");
// Serial.print(audioPresent);

// Serial.print("  Bands: ");

// for (int i = 0; i < xres; i++) {
//     Serial.print(data_avgs[i], 1);
//     Serial.print(" ");
// }

// Serial.println();
// //  until here debug
    if ((millis() - lastAudioTime) > 5000)
    {
        static unsigned long idleTimer = 0;

        if (millis() - idleTimer > 200)     // breathing speed
        {
            idleTimer = millis();

            breatheIndex++;

            if (breatheIndex >= breatheSteps)
                breatheIndex = 0;
        }

        mx.clear();

        // centre four LEDs always ON
        mx.setPoint(3, 3, true);
        mx.setPoint(3, 4, true);
        mx.setPoint(4, 3, true);
        mx.setPoint(4, 4, true);

        mx.control(MD_MAX72XX::INTENSITY,
                breatheTable[breatheIndex]);
        mx.update();
        return;
    }

    mx.clear();

    // centre four LEDs on an 8x8 display
    mx.setPoint(3, 3, idleLevel > 0);
    mx.setPoint(3, 4, idleLevel > 0);
    mx.setPoint(4, 3, idleLevel > 0);
    mx.setPoint(4, 4, idleLevel > 0);

    mx.control(MD_MAX72XX::INTENSITY, idleLevel);

    return;
}

    // restore LED brightness
mx.control(MD_MAX72XX::INTENSITY, 1);

    // Map values to display and update LED matrix
    for (int i = 0; i < xres; i++) {
        // Constrain and map to 0-8 range for display
        float level = data_avgs[i];

        if (level < 3)
        level = 0;

        if (level > 120)
            level = 120;

        yvalue = map((int)level, 0, 120, 0, yres);

        // Peak decay effect - slowly lower peaks over time
if (peakDecayTime == 0)
{
    peaks[i] = yvalue;
}
else
{
    static unsigned long lastDecay = 0;

    if (millis() - lastDecay >= peakDecayTime)
    {
        lastDecay = millis();

        for (int p = 0; p < xres; p++)
        {
            if (peaks[p] > 0)
                peaks[p]--;
        }
    }

    if (yvalue > peaks[i])
        peaks[i] = yvalue;

    yvalue = peaks[i];
}
        // Convert to display value and update column
        displayvalue = MY_ARRAY[yvalue];
        // displayvalue = 0;
        // for (int b = 0; b < 8; b++)
        // {
        //     if (MY_ARRAY[yvalue] & (1 << b))
        //         displayvalue |= (1 << (7 - b));
        // }
        displaycolumn = 7 - i;      // Changed from 31-i to 7-i for 8 columns
        mx.setRow(displaycolumn, displayvalue);
    }
}