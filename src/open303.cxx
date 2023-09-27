// Copyright (C)2022 - Eduard Heidt
//
// Author: Eduard Heidt (eh2k@gmx.de)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//

#include "stmlib/stmlib.h"
#include "machine.h"
#include "open303/src/rosic_Open303.h"
#include <bitset>
#define protected public
#include "open303/src/wavetable_gen/rosic_MipMappedWaveTable.h"

#ifndef FLASHMEM
#include "pgmspace.h"
#endif

using namespace machine;
using namespace rosic;

struct Open303Engine : public Engine, rosic::Open303
{
    Open303Engine() : Engine(TRIGGER_INPUT | VOCT_INPUT), rosic::Open303(nullptr, nullptr)
    {
        Open303::setSampleRate(machine::SAMPLE_RATE, 1);
        Open303::setWaveform(_waveform);
        Open303::setTuning(Open303::tuning);
        Open303::setAmpSustain(0);
        Open303::setAccentAttack(Open303::accentAttack);
        Open303::setPitchBend(0);
        Open303::setSlideTime(Open303::slideTime);
        Open303::setVolume(-12);
        Open303::filter.setMode(TeeBeeFilter::TB_303);

        param[0].init_v_oct("Freq", &_pitch);
        param[1].init("Acc", &_acc, 100, 1.f, 100.f);
        param[2].init("Cutoff", &_cutoff, 0.25f);
        param[3].init("Reso", &_res, 33.f, 1.f, 100.f);
        param[4].init("EnvMod", &_env, 25.f, 1.f, 100.f);
        param[5].init("Dec", &_dec, 0.5f);
        // param[6].init("WAVEFORM", &_waveform, _waveform, 0, 1);
        // param[6].print_value = [&](char *val)
        // {
        //     sprintf(val, _waveform ? "SQR" : "SAW");
        // };
        _square = (WaveTable *)machine::malloc(sizeof(WaveTable));
        if (_square)
        {
            MipMappedWaveTable tmp;
            tmp.fillWithSquare303();
            tmp.generateMipMap(*_square);
            oscillator.setWaveTables(_square, _square);
        }
    }

    WaveTable *_square = nullptr;

    ~Open303Engine() override
    {
        machine::mfree(_square);
    }
    float buffer[FRAME_BUFFER_SIZE];

    float _pitch = -1;
    float _acc = 0;
    float _cutoff = 0;
    float _res = 0;
    float _env = 0;
    float _dec = 0;
    float _note;
    uint8_t _waveform = 1;
    bool _gate;

    void process(const ControlFrame &frame, OutputFrame &of) override
    {
        if (_square == nullptr)
            return;

        Open303::setWaveform(_waveform);
        Open303::setAccent(_acc);
        Open303::setCutoff(linToExp(_cutoff, 0.0, 1.0, 314.0, 2394.0));
        Open303::setResonance(_res);
        Open303::setEnvMod(_env);
        Open303::setDecay(linToExp(_dec, 0.0, 1.0, 200.0, 2000.0));
        if (_acc > 0)
            Open303::setAccentDecay(linToExp(_dec, 0.0, 1.0, 200.0, 2000.0));

        _note = (float)machine::DEFAULT_NOTE + 24 + frame.qz_voltage(this->io, _pitch) * 12;
        CONSTRAIN(_note, 0, 128);

        _gate |= frame.gate;

        if (frame.trigger)
            Open303::triggerNote(_note, _acc > 0);
        else if (!frame.gate)
        {
            Open303::releaseNote(_note);
            _gate = false;
        }
        else
            Open303::oscFreq = pitchToFreq(_note, tuning);

        for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
            buffer[i] = Open303::getSample();

        of.push(buffer, machine::FRAME_BUFFER_SIZE);
    }

    void display() override
    {
        gfx::drawEngine(this, _square ? nullptr : machine::OUT_OF_MEMORY);
    }

    uint32_t _r = 0;
    void display_screensaver() override
    {
        static const uint8_t _screenSaver[] FLASHMEM = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x3f, 0x00, 0x00, //>
            0x00, 0x80, 0x0f, 0xe0, 0x01, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x07, 0x00,
            0x00, 0x30, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x30, 0x00,
            0x00, 0x06, 0x00, 0x00, 0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x80, 0x00,
            0x80, 0x01, 0x06, 0xc0, 0x00, 0x01, 0xc0, 0x00, 0x0f, 0xe0, 0x01, 0x03,
            0x60, 0x00, 0x0f, 0xe0, 0x01, 0x06, 0x30, 0x80, 0x1f, 0xf0, 0x03, 0x0c,
            0x10, 0x80, 0x1f, 0xf0, 0x03, 0x08, 0x18, 0x80, 0x1f, 0xf0, 0x03, 0x18,
            0x08, 0x80, 0x1f, 0xf0, 0x03, 0x10, 0x08, 0x80, 0x1f, 0xf0, 0x03, 0x30,
            0x04, 0x80, 0x1f, 0xf0, 0x03, 0x20, 0x04, 0x80, 0x1f, 0xf0, 0x03, 0x20,
            0x04, 0x80, 0x1f, 0xf0, 0x03, 0x60, 0x06, 0x00, 0x0f, 0xe0, 0x01, 0x40,
            0x02, 0x00, 0x0f, 0xe0, 0x01, 0x40, 0x02, 0x00, 0x06, 0xc0, 0x00, 0x40,
            0x02, 0x00, 0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x40,
            0xe2, 0x07, 0x00, 0x00, 0xe0, 0x47, 0xc2, 0x03, 0x00, 0x00, 0xc0, 0x43,
            0x82, 0x01, 0x00, 0x00, 0x80, 0x41, 0x82, 0x01, 0x00, 0x00, 0x80, 0x41,
            0x86, 0x01, 0x00, 0x00, 0xc0, 0x60, 0x04, 0x03, 0x00, 0x00, 0xc0, 0x60,
            0x04, 0x07, 0x00, 0x00, 0xc0, 0x20, 0x0c, 0x06, 0x00, 0x00, 0x60, 0x20,
            0x08, 0x0e, 0x00, 0x00, 0x70, 0x30, 0x08, 0x1c, 0x00, 0x00, 0x30, 0x10,
            0x18, 0x38, 0x00, 0x00, 0x1c, 0x18, 0x10, 0x70, 0x00, 0x00, 0x0e, 0x08,
            0x20, 0xe0, 0x00, 0x00, 0x0f, 0x0c, 0x60, 0xc0, 0x03, 0xc0, 0x03, 0x06,
            0xc0, 0x00, 0x0f, 0xf0, 0x00, 0x02, 0x80, 0x01, 0xfc, 0x3f, 0x00, 0x01,
            0x00, 0x03, 0xe0, 0x07, 0xc0, 0x00, 0x00, 0x06, 0x00, 0x00, 0x60, 0x00,
            0x00, 0x0c, 0x00, 0x00, 0x30, 0x00, 0x00, 0x30, 0x00, 0x00, 0x1c, 0x00,
            0x00, 0xe0, 0x00, 0x00, 0x07, 0x00, 0x00, 0x80, 0x0f, 0xf0, 0x01, 0x00,
            0x00, 0x00, 0xf8, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0xf8, 0x3f, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x01, 0x00, //>
            0x00, 0xe0, 0xff, 0xff, 0x07, 0x00, 0x00, 0xf0, 0x0f, 0xe0, 0x1f, 0x00,
            0x00, 0xfc, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x7c, 0x00,
            0x00, 0x0f, 0x00, 0x00, 0xf0, 0x00, 0x80, 0x07, 0x06, 0xc0, 0xe0, 0x01,
            0xc0, 0x03, 0x0f, 0xe0, 0x81, 0x03, 0xe0, 0x81, 0x1f, 0xf0, 0x83, 0x07,
            0xf0, 0x80, 0x1f, 0xf0, 0x03, 0x0f, 0x78, 0xc0, 0x3f, 0xf8, 0x07, 0x1e,
            0x38, 0xc0, 0x3f, 0xf8, 0x07, 0x1c, 0x3c, 0xc0, 0x3f, 0xf8, 0x07, 0x3c,
            0x1c, 0xc0, 0x3f, 0xf8, 0x07, 0x38, 0x1c, 0xc0, 0x3f, 0xf8, 0x07, 0x78,
            0x0e, 0xc0, 0x3f, 0xf8, 0x07, 0x70, 0x0e, 0xc0, 0x3f, 0xf8, 0x07, 0x70,
            0x0e, 0xc0, 0x1f, 0xf0, 0x07, 0xf0, 0x0f, 0x80, 0x1f, 0xf0, 0x03, 0xe0,
            0x07, 0x80, 0x0f, 0xe0, 0x03, 0xe0, 0x07, 0x00, 0x0f, 0xe0, 0x01, 0xe0,
            0x07, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xe7, 0x07, 0x00, 0x00, 0xe0, 0xe7,
            0xf7, 0x07, 0x00, 0x00, 0xe0, 0xef, 0xe7, 0x07, 0x00, 0x00, 0xe0, 0xe7,
            0xc7, 0x03, 0x00, 0x00, 0xc0, 0xe3, 0xc7, 0x03, 0x00, 0x00, 0xc0, 0xe3,
            0xcf, 0x03, 0x00, 0x00, 0xe0, 0xf1, 0x8e, 0x07, 0x00, 0x00, 0xe0, 0xf1,
            0x8e, 0x0f, 0x00, 0x00, 0xe0, 0x71, 0x1e, 0x0f, 0x00, 0x00, 0xf0, 0x70,
            0x1c, 0x1f, 0x00, 0x00, 0xf8, 0x78, 0x1c, 0x3e, 0x00, 0x00, 0x7c, 0x38,
            0x3c, 0x7c, 0x00, 0x00, 0x3e, 0x3c, 0x38, 0xf8, 0x00, 0x00, 0x1f, 0x1c,
            0x70, 0xf0, 0x03, 0xc0, 0x1f, 0x1e, 0xf0, 0xe0, 0x0f, 0xf0, 0x0f, 0x0f,
            0xe0, 0xc1, 0xff, 0xff, 0x03, 0x07, 0xc0, 0x03, 0xff, 0xff, 0xc0, 0x03,
            0x80, 0x07, 0xfc, 0x3f, 0xe0, 0x01, 0x00, 0x0f, 0xe0, 0x07, 0xf0, 0x00,
            0x00, 0x3e, 0x00, 0x00, 0x7c, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x3f, 0x00,
            0x00, 0xf0, 0x0f, 0xf0, 0x1f, 0x00, 0x00, 0xe0, 0xff, 0xff, 0x07, 0x00,
            0x00, 0x80, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0xf8, 0x3f, 0x00, 0x00};

        memset(gfx::display_buffer, 0, 1024);

        for (int i = 23; i > 0; i--)
        {
            if ((_r & (1 << i)))
                gfx::drawCircle(64, 32, 26 + powf(i, 1.3f));
        }

        if (_gate)
        {
            _r |= 1;
            gfx::drawXbm(40, 8, 48, 48, &_screenSaver[sizeof(_screenSaver) / 2]);
        }
        else
            gfx::drawXbm(40, 8, 48, 48, &_screenSaver[0]);

        _r <<= 1;
    }
};

void init_open303()
{
    machine::add<Open303Engine>(machine::SYNTH, "Open303");
}

MACHINE_INIT(init_open303);
