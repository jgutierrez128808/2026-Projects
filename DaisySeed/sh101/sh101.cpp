// SH-101 inspired hybrid synthesizer for the Daisy Seed.
//
// The Daisy generates everything digitally except the filter: a single VCO
// (saw + pulse with PWM), a sub-oscillator, noise, a source mixer, one ADSR,
// one LFO and a digital VCA. The VCF is an external analog filter.
//
// Signal flow:
//   AUDIO OUT L  -> analog VCF input
//   AUDIO IN  L  <- analog VCF output (filtered signal returns here)
//   AUDIO OUT R  -> final output to amp (digital VCA applied on the return)
//   DAC1 (pin 22) -> cutoff CV (cutoff knob + env + LFO + keytrack)
//   DAC2 (pin 23) -> resonance CV
//
// Panel controls are read through three CD4051 8-channel analog muxes:
//   ADC inputs : A0 (mux0), A1 (mux1), A2 (mux2)
//   select pins: D9, D10, D11 (shared address lines to all three muxes)
//
// MIDI: native USB and TRS via UART (USART1, default pins) are both read.
//
// Bench tip: with no analog filter connected, patch OUT L -> IN L to hear the
// raw (unfiltered) voice through the VCA.

#include "daisy_seed.h"
#include "daisysp.h"
#include "src/Params.h"
#include "src/Voice.h"
#include "src/Controls.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;
using namespace sh101;

DaisySeed       hw;
Voice           voice;
SynthParams     params;
MidiUsbHandler  midi_usb;
MidiUartHandler midi_uart;

// Monophonic last-note priority stack.
static constexpr int kMaxHeld = 16;
uint8_t              held_notes[kMaxHeld];
int                  held_count = 0;

// CV values written to the DACs each audio block.
volatile uint16_t cutoff_cv = 0;
volatile uint16_t res_cv    = 0;

static inline uint16_t To12Bit(float v)
{
    v = fclamp(v, 0.0f, 1.0f);
    return static_cast<uint16_t>(v * 4095.0f);
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    float cutoff = 0.0f;
    for(size_t i = 0; i < size; i++)
    {
        float src = voice.Process(); // source mix -> analog VCF
        float env = voice.Env();
        float lfo = voice.LfoValue();

        out[0][i] = src; // OUT L -> analog VCF input

        // Digital VCA applied to the filtered signal returning on IN L.
        float vca = (params.vcaMode == VcaMode::Env) ? env
                                                      : (voice.Gate() ? 1.0f : 0.0f);
        out[1][i] = in[0][i] * vca; // OUT R -> amp

        // Cutoff CV (block-rate update is plenty for a filter sweep).
        float kt = params.keyTrack * ((voice.Note() - 60.0f) / 60.0f);
        cutoff   = params.cutoff + params.envAmount * env
                 + params.vcfLfoDepth * (lfo * 0.5f + 0.5f) + kt;
    }

    cutoff_cv = To12Bit(cutoff);
    res_cv    = To12Bit(params.resonance);
    hw.dac.WriteValue(DacHandle::Channel::ONE, cutoff_cv);
    hw.dac.WriteValue(DacHandle::Channel::TWO, res_cv);
}

static void NoteOnStack(uint8_t note)
{
    if(held_count < kMaxHeld)
        held_notes[held_count++] = note;
    voice.NoteOn(static_cast<float>(note));
}

static void NoteOffStack(uint8_t note)
{
    for(int i = 0; i < held_count; i++)
    {
        if(held_notes[i] == note)
        {
            for(int j = i; j < held_count - 1; j++)
                held_notes[j] = held_notes[j + 1];
            held_count--;
            break;
        }
    }
    if(held_count > 0)
        voice.NoteOn(static_cast<float>(held_notes[held_count - 1]));
    else
        voice.NoteOff();
}

static void HandleMidi(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
            NoteOnEvent e = m.AsNoteOn();
            if(e.velocity == 0)
                NoteOffStack(e.note);
            else
                NoteOnStack(e.note);
        }
        break;
        case NoteOff: NoteOffStack(m.AsNoteOff().note); break;
        case PitchBend:
            voice.SetBend((m.AsPitchBend().value / 8192.0f) * 2.0f); // +/- 2 semitones
            break;
        default: break;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    float sr = hw.AudioSampleRate();

    // Panel control muxes.
    AdcChannelConfig adc_cfg[3];
    adc_cfg[0].InitMux(A0, 8, D9, D10, D11);
    adc_cfg[1].InitMux(A1, 8, D9, D10, D11);
    adc_cfg[2].InitMux(A2, 8, D9, D10, D11);
    hw.adc.Init(adc_cfg, 3);
    hw.adc.Start();

    // CV outputs.
    DacHandle::Config dac_cfg;
    dac_cfg.bitdepth   = DacHandle::BitDepth::BITS_12;
    dac_cfg.buff_state = DacHandle::BufferState::ENABLED;
    dac_cfg.mode       = DacHandle::Mode::POLLING;
    dac_cfg.chn        = DacHandle::Channel::BOTH;
    hw.dac.Init(dac_cfg);

    // MIDI in (USB + TRS/UART).
    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbHandler::Config::INTERNAL;
    midi_usb.Init(midi_cfg);
    MidiUartHandler::Config uart_cfg;
    midi_uart.Init(uart_cfg);

    voice.Init(sr);
    voice.SetParams(&params);

    midi_usb.StartReceive();
    midi_uart.StartReceive();
    hw.StartAudio(AudioCallback);

    while(1)
    {
        ProcessControls(hw, params);

        midi_usb.Listen();
        while(midi_usb.HasEvents())
            HandleMidi(midi_usb.PopEvent());

        midi_uart.Listen();
        while(midi_uart.HasEvents())
            HandleMidi(midi_uart.PopEvent());
    }
}
