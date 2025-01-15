#include <stdio.h>
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>


void init_sawtooth_wave() {

    DDRD |= (1 << PD3);  //OC2B pin D3
    TCCR2A |= (1 << WGM21) | (1 << WGM20);  // Fast PWM mode
    TCCR2A |= (1 << COM2B1);// Non-inverting mode
    TCCR2B |= (1 << CS20);   // No prescaling
}

void init_square_wave() {

    DDRD |= (1 << PD6);   // Port D6 is the OCOA pin
    // Set Fast PWM mode (Mode 3)  Page 86
    TCCR0A |= (1 << WGM01) | (1 << WGM00);
    TCCR0A |= (1 << COM0A1);  //  Clear OC0A on Compare Match,
    // set at BOTTOM  (Non Inverting mode page 84)

    // Set prescaler to 256   page 87
    TCCR0B |= (1 << CS02);

    OCR0A = 127;
 // Set duty cycle to 50%
}

int main(void) {

    DDRB = 0xFF;  // Set PORTB as output for sine wave
    uint8_t i = 0;
    uint8_t duty_cycle = 0;

    // Sine wave lookup table
    unsigned int data[] = {
        128, 136, 143, 151, 159, 167, 174, 182,
        189, 196, 202, 209, 215, 220, 226, 231,
        235, 239, 243, 246, 249, 251, 253, 254,
        255, 255, 255, 254, 253, 251, 249, 246,
        243, 239, 235, 231, 226, 220, 215, 209,
        202, 196, 189, 182, 174, 167, 159, 151,
        143, 136, 128, 119, 112, 104, 96, 88,
        81, 73, 66, 59, 53, 46, 40, 35,
        29, 24, 20, 16, 12, 9, 6, 4,
        2, 1, 0, 0, 0, 1, 2, 4,
        6, 9, 12, 16, 20, 24, 29, 35,
        40, 46, 53, 59, 66, 73, 81, 88,
        96, 104, 112, 119
    };

    init_sawtooth_wave();
    init_square_wave();

    while (1) {
        // Update sawtooth wave
         OCR2B = duty_cycle;
         duty_cycle++;

        // Generate sine wave
        for (i = 0; i < 100; i++) {
            PORTB = data[i];
        }
    }

    return 0;
}
