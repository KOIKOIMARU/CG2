"""ゲーム用の短いオリジナル効果音を生成。外部音源・第三者素材は使用しない。"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 48000
OUT = Path(__file__).resolve().parents[1] / "resources" / "audio" / "combat"


def render(name, duration, kind):
    rng = random.Random(924 + kind)
    values = []
    low_noise = 0.0
    phase = 0.0
    for i in range(round(duration * RATE)):
        t = i / RATE
        u = t / duration
        noise = rng.uniform(-1, 1)
        low_noise += 0.10 * (noise - low_noise)
        attack = min(1.0, t / 0.003)
        tail = min(1.0, (duration - t) / 0.025)
        env = attack * tail * math.exp(-4.8 * u)
        if kind in (0, 1):
            # 射撃は短い下降音。チャージ弾だけ低域と空気感を足す。
            freq = (1300 if kind == 0 else 680) * math.exp(-5 * u) + 110
            phase += 2 * math.pi * freq / RATE
            value = 0.65 * math.sin(phase + 0.5 * math.sin(phase * 2.02))
            value += 0.20 * noise * math.exp(-24 * u) + 0.35 * low_noise
        elif kind == 2:
            value = 0.6 * noise * math.exp(-9 * u) + 0.25 * math.sin(2 * math.pi * 1780 * t)
        elif kind in (3, 4, 11):
            freq = (130 if kind == 3 else 240) * math.exp(-3 * u) + 38
            phase += 2 * math.pi * freq / RATE
            value = 1.5 * low_noise + 0.45 * math.sin(phase) + 0.2 * noise * math.exp(-15 * u)
        elif kind in (5, 6, 7, 9):
            notes = {5: (440, 554.37, 659.25, 880), 6: (523.25, 659.25, 783.99, 1046.5), 7: (330, 277.18, 220, 164.81), 9: (659.25, 987.77, 1318.51, 1975.53)}[kind]
            value = 0.0
            for n, freq in enumerate(notes):
                local = t - n * duration * 0.14
                if local >= 0:
                    note_env = min(1.0, local / 0.007) * math.exp(-local * 9)
                    value += 0.3 * note_env * (math.sin(2 * math.pi * freq * local) + 0.16 * math.sin(4 * math.pi * freq * local))
            env = attack * tail
        elif kind == 10:
            phase += 2 * math.pi * (1700 * math.exp(-9 * u) + 180) / RATE
            value = 0.75 * (noise - low_noise) + 0.35 * math.sin(phase)
            env = min(1.0, t / 0.009) * tail * math.exp(-7 * u)
        else:
            value = 1.3 * low_noise + 0.1 * math.sin(2 * math.pi * (600 * t + 600 * t * t))
            env = math.sin(math.pi * u) ** 2 * tail
        values.append(value * env)
    # DC除去、十分なヘッドルーム、両端を無音にしてクリックを防ぐ。
    mean = sum(values) / len(values)
    values = [v - mean for v in values]
    peak = max(abs(v) for v in values)
    gain = 0.42 / max(peak, 0.001)
    samples = [round(v * gain * 32767) for v in values]
    samples[0] = samples[-1] = 0
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / f"{name}.wav"), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(struct.pack(f"<{len(samples)}h", *samples))
    assert max(abs(v) for v in samples) < 32767
    print(name, len(samples), "samples", "peak", max(abs(v) for v in samples))


if __name__ == "__main__":
    for args in [("shot", .10, 0), ("charge", .22, 1), ("hit", .065, 2),
                 ("destroy", .34, 3), ("damage", .18, 4), ("fever", .52, 5),
                 ("clear", .72, 6), ("fail", .54, 7), ("dodge", .15, 8),
                 ("skill_ready", .28, 9), ("slash", .16, 10), ("slash_finish", .40, 11)]:
        render(*args)
