#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

void sawtooth_wave() {
    DDRD |= (1 << PD3);  // Set PD6 as output (OC0A)

    // Configure Timer0 for Fast PWM mode
    TCCR2A |= (1 << WGM21) | (1 << WGM20);  // Fast PWM mode
    TCCR2A |= (1 << COM2B1);
                  // Non-inverting mode
    TCCR2B |= (1 << CS20);                 // No prescaling

    uint8_t duty_cycle = 0;

    while (1) {
        OCR2B = duty_cycle;
        _delay_ms(1);        // Fixed the delay syntax
        duty_cycle++;
    }
}

int main(void) {
    sawtooth_wave();
    return 0;
}
