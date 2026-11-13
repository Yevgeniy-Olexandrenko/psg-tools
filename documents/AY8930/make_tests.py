"""Makes the PSG test files for the AY8930 tone at short periods.

Each test plays an audible triangle envelope on channel A with the tone of the
channel enabled. The envelope is the sound; the tone only gates it. At short
tone periods the tone is ultrasonic, so the loudness of the envelope shows the
mean of the gate. Each step plays 1 s, then 0.5 s of silence.

The files go to the 'tests' folder next to this script:
  tone_exp_duty8.psg   expanded mode, duty 96.875 %, TP = 0, 1, 2, 3, 4, 8, 16, 31, 32, 64
  tone_exp_duty4.psg   expanded mode, duty 50 %, the same periods
  tone_compat.psg      compatibility mode, TP = 0, 1, 2, 3, 4, 8, 16, then tone off
"""

import os

FPS = 50
STEP_ON = 50
STEP_OFF = 25

EXP_PERIODS = [0, 1, 2, 3, 4, 8, 16, 31, 32, 64]
COMPAT_PERIODS = [0, 1, 2, 3, 4, 8, 16]

# envelope: triangle (shape 0Eh), period 16, about 200 Hz at 1.77 MHz
ENV_SHAPE = 0x0E
ENV_PERIOD = 16


class PSG:
    def __init__(self):
        self.data = bytearray(b"PSG\x1A" + bytes([10, FPS]) + bytes(10))

    def frame(self, writes=()):
        self.data.append(0xFF)
        for reg, value in writes:
            self.data += bytes([reg, value])

    def wait(self, frames):
        for _ in range(frames):
            self.frame()

    def save(self, path):
        with open(path, "wb") as f:
            f.write(self.data)


def tone_steps(psg, periods, volume_on):
    for tp in periods:
        psg.frame([(0x00, tp & 0xFF), (0x01, tp >> 8), (0x08, volume_on)])
        psg.wait(STEP_ON - 1)
        psg.frame([(0x08, 0x00)])
        psg.wait(STEP_OFF - 1)


def expanded(duty):
    psg = PSG()
    psg.frame([
        (0x0D, 0xA0 | ENV_SHAPE),        # expanded mode, envelope A shape
        (0x0B, ENV_PERIOD), (0x0C, 0),   # envelope A period
        (0x16, duty),                    # duty A (bank B, flat number)
        (0x07, 0x3E),                    # tone A only
        (0x08, 0x00),
    ])
    psg.wait(STEP_OFF - 1)
    tone_steps(psg, EXP_PERIODS, 0x20)   # amplitude from envelope A
    return psg


def compat():
    psg = PSG()
    psg.frame([
        (0x0D, ENV_SHAPE),               # compatibility mode
        (0x0B, ENV_PERIOD), (0x0C, 0),
        (0x07, 0x3E),
        (0x08, 0x00),
    ])
    psg.wait(STEP_OFF - 1)
    tone_steps(psg, COMPAT_PERIODS, 0x10)
    # tone and noise off: silent on the AY8930, the full envelope on the AY-3-8910
    psg.frame([(0x07, 0x3F), (0x08, 0x10)])
    psg.wait(STEP_ON - 1)
    psg.frame([(0x08, 0x00)])
    psg.wait(STEP_OFF - 1)
    return psg


def main():
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tests")
    os.makedirs(out, exist_ok=True)
    expanded(8).save(os.path.join(out, "tone_exp_duty8.psg"))
    expanded(4).save(os.path.join(out, "tone_exp_duty4.psg"))
    compat().save(os.path.join(out, "tone_compat.psg"))


if __name__ == "__main__":
    main()
