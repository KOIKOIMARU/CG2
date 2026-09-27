"""CC0素材を編集・重ね合わせして、SKYBREAKの戦闘SEへ仕上げる。

python GenerateCombatSounds.py <展開済み素材フォルダ> [--preview <試聴wav>]
オフライン専用: numpy / scipy / soundfile。ゲームは完成済みPCMだけを使う。
素材の取得先は resources/audio/combat/CREDITS.md を参照。
"""
import argparse
import hashlib
import json
import math
import shutil
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy.signal import butter, resample_poly, sosfilt

RATE = 48000
OUTPUT = Path(__file__).resolve().parents[1] / 'resources/audio/combat'
PACKS = {'sci': 'kenney_scifi/Audio', 'metal': 'kenney_impact/Audio',
         'rpg': 'kenney_rpg/Audio', 'ui': 'kenney_interface/Audio', 'duck': 'rubberduck_scifi'}


class Editor:
    def __init__(self, source):
        self.source, self.sources, self.recipes, self.current = source, {}, {}, None

    def clip(self, pack, name, *, speed=1.0, duration=None, start=0.0,
             high=45.0, low=12000.0, width=0.6):
        relative = f'{PACKS[pack]}/{name}.ogg'
        path = self.source / relative
        if relative not in self.sources:
            self.sources[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
        self.recipes[self.current].append(relative)
        data, rate = sf.read(path, always_2d=True, dtype='float64')
        if data.shape[1] == 1:
            data = np.repeat(data, 2, axis=1)
        assert data.shape[1] == 2 and np.isfinite(data).all()
        gcd = math.gcd(RATE, rate)
        data = resample_poly(data, RATE // gcd, rate // gcd, axis=0)
        # 先頭の録音待ちだけ除去し、風切り音などの立ち上がりは残す。
        threshold = max(0.0001, float(abs(data).max()) * 0.006)
        active = np.flatnonzero(abs(data).max(axis=1) >= threshold)
        if not len(active):
            raise ValueError(f'Silent source: {path}')
        data = data[max(0, int(active[0]) - 96):]
        data = data[round(start * RATE):]
        data = resample_poly(data, 1000, round(speed * 1000), axis=0)
        if duration is not None:
            data = data[:round(duration * RATE)]
        assert len(data) > 64
        data = sosfilt(butter(2, high, 'highpass', fs=RATE, output='sos'), data, axis=0)
        data = sosfilt(butter(2, low, 'lowpass', fs=RATE, output='sos'), data, axis=0)
        middle = data.mean(axis=1, keepdims=True)
        data = middle + (data - middle) * width
        data /= max(float(abs(data).max()), 0.001)
        attack, tail = min(96, len(data) // 4), min(round(0.025 * RATE), len(data) // 4)
        data[:attack] *= np.linspace(0, 1, attack)[:, None]
        data[-tail:] *= np.linspace(1, 0, tail)[:, None]
        return data

    def begin(self, name):
        self.current = name
        self.recipes[name] = []

    def finish(self, name, seconds, layers, *, peak=0.63, room=0.0):
        result = np.zeros((round(seconds * RATE), 2))
        for clip, gain, offset in layers:
            first = round(offset * RATE)
            count = min(len(clip), len(result) - first)
            result[first:first + count] += gain * clip[:count]
        # 連射は乾いた短音、撃破・決め技だけに短い反射音を足す。
        if room:
            dry = result.copy()
            for delay, level in ((0.029, 0.23), (0.047, 0.15), (0.079, 0.08)):
                n = round(delay * RATE)
                result[n:] += dry[:-n, ::-1] * room * level
        result = sosfilt(butter(2, 35, 'highpass', fs=RATE, output='sos'), result, axis=0)
        result *= peak / max(float(abs(result).max()), 1e-9)
        result[:96] *= np.linspace(0, 1, 96)[:, None]
        tail = min(round(0.060 * RATE), len(result) // 4)
        result[-tail:] *= np.linspace(1, 0, tail)[:, None]
        result[0] = result[-1] = 0
        assert np.isfinite(result).all() and abs(result).max() <= 0.72
        OUTPUT.mkdir(parents=True, exist_ok=True)
        sf.write(OUTPUT / f'{name}.wav', result, RATE, subtype='PCM_16')
        print(f'{name:14s} {seconds:.3f}s peak={abs(result).max():.3f} rms={np.sqrt(np.mean(result ** 2)):.3f}')
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--preview', type=Path)
    args = parser.parse_args()
    e, results = Editor(args.source), {}
    c = e.clip
    for take in range(3):
        suffix = '' if take == 0 else f'_{take + 1:02d}'
        name = 'shot' + suffix
        e.begin(name)
        results[name] = e.finish(name, 0.19, [
            (c('sci', f'laserSmall_{take:03d}', speed=1.12, duration=0.18, low=7500, width=0.25), 0.64, 0),
            (c('duck', 'shoot_02', speed=0.85 + 0.04 * take, duration=0.13, low=4800, width=0.2), 0.25, 0),
            (c('metal', f'impactMetal_heavy_{[0,2,4][take]:03d}', speed=1.1, duration=0.07, low=2400), 0.12, 0),
        ], peak=0.59)
        name = 'hit' + suffix
        e.begin(name)
        results[name] = e.finish(name, 0.20, [
            (c('metal', f'impactMetal_medium_{[1,2,4][take]:03d}', duration=0.17, high=180, low=7800), 0.80, 0),
            (c('metal', f'impactGlass_light_{take:03d}', speed=1.3, duration=0.11, high=1700, low=9000), 0.16, 0.005),
        ], peak=0.60)
        name = 'destroy' + suffix
        e.begin(name)
        results[name] = e.finish(name, 0.68, [
            (c('sci', f'explosionCrunch_{[0,2,3][take]:03d}', speed=0.95 + 0.04 * take, duration=0.61, low=6000), 0.62, 0),
            (c('sci', 'lowFrequency_explosion_001', speed=1.08 + 0.05 * take, duration=0.48, high=40, low=650), 0.45, 0),
            (c('metal', f'impactMetal_heavy_{[0,2,4][take]:03d}', duration=0.12, high=400, low=5000), 0.20, 0),
        ], peak=0.65, room=0.5)
        name = 'slash' + suffix
        e.begin(name)
        results[name] = e.finish(name, 0.24, [
            (c('rpg', 'knifeSlice2' if take == 1 else 'knifeSlice', start=0.12, speed=1.35 + 0.09 * take,
               duration=0.23, high=200, low=9000, width=0.8), 0.65, 0),
            (c('rpg', f'drawKnife{take+1}', speed=1.35, duration=0.16, high=950, low=10000), 0.22, 0),
            (c('metal', f'impactMetal_light_{take:03d}', speed=1.35, duration=0.1, high=1500, low=8000), 0.12, 0.035),
        ], peak=0.62)
    e.begin('charge')
    results['charge'] = e.finish('charge', 0.50, [
        (c('sci', 'laserLarge_002', speed=0.93, duration=0.46, low=7500), 0.68, 0),
        (c('sci', 'lowFrequency_explosion_001', speed=1.25, duration=0.32, low=520), 0.35, 0),
        (c('metal', 'impactMetal_heavy_002', duration=0.08, low=4000), 0.15, 0),
    ], peak=0.64, room=0.15)
    e.begin('damage')
    results['damage'] = e.finish('damage', 0.42, [
        (c('sci', 'impactMetal_003', speed=0.85, duration=0.30, high=120, low=5300), 0.65, 0),
        (c('metal', 'impactPlate_heavy_001', speed=0.85, duration=0.2, low=3500), 0.38, 0),
        (c('ui', 'error_006', duration=0.21, low=4000), 0.18, 0.075),
    ], peak=0.68)
    e.begin('dodge')
    results['dodge'] = e.finish('dodge', 0.29, [
        (c('rpg', 'knifeSlice', start=0.08, speed=1.2, duration=0.28, high=280, low=6500, width=0.9), 0.7, 0),
        (c('sci', 'thrusterFire_002', start=0.03, speed=1.25, duration=0.24, high=650, low=4800), 0.21, 0),
    ], peak=0.56)
    e.begin('slash_finish')
    results['slash_finish'] = e.finish('slash_finish', 0.95, [
        (c('rpg', 'knifeSlice2', start=0.12, speed=0.82, duration=0.48, high=200, low=9200, width=1.0), 0.56, 0),
        (c('sci', 'lowFrequency_explosion_000', speed=1.22, duration=0.82, high=40, low=1000, width=0.65), 0.53, 0.018),
        (c('sci', 'impactMetal_002', speed=0.86, duration=0.4, high=600, low=8200), 0.27, 0.020),
    ], peak=0.70, room=0.8)
    e.begin('fever')
    results['fever'] = e.finish('fever', 1.05, [
        (c('sci', 'forceField_004', speed=1.35, duration=0.70, high=120, low=7000), 0.58, 0),
        (c('sci', 'lowFrequency_explosion_001', speed=0.9, duration=0.55, low=500), 0.34, 0),
        (c('ui', 'confirmation_002', speed=0.9, duration=0.5, high=350, low=7500, width=0.7), 0.32, 0.18),
    ], peak=0.66, room=0.6)
    e.begin('skill_ready')
    results['skill_ready'] = e.finish('skill_ready', 0.34, [
        (c('ui', 'confirmation_001', duration=0.31, high=250, low=7000), 0.74, 0),
        (c('rpg', 'drawKnife1', speed=1.55, duration=0.16, high=1400, low=8500), 0.12, 0.025),
    ], peak=0.50)
    e.begin('clear')
    results['clear'] = e.finish('clear', 1.00, [
        (c('ui', 'confirmation_004', speed=0.85, duration=0.75, high=120, low=7500), 0.70, 0),
        (c('ui', 'glass_004', speed=0.80, duration=0.65, high=250, low=7200), 0.26, 0.09),
    ], peak=0.61, room=0.8)
    e.begin('fail')
    results['fail'] = e.finish('fail', 0.85, [
        (c('ui', 'error_004', speed=0.75, duration=0.72, high=100, low=3600), 0.56, 0),
        (c('sci', 'forceField_000', speed=0.80, duration=0.55, high=100, low=1300), 0.22, 0.04),
    ], peak=0.57)
    licenses = OUTPUT / 'licenses'
    licenses.mkdir(exist_ok=True)
    for pack in ('kenney_scifi', 'kenney_impact', 'kenney_rpg', 'kenney_interface'):
        shutil.copyfile(args.source / pack / 'License.txt', licenses / f'{pack}.txt')
    (OUTPUT / 'source_manifest.json').write_text(json.dumps({
        'sources_sha256': e.sources, 'output_sources': e.recipes,
        'format': '48000 Hz, stereo, PCM16',
    }, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if args.preview:
        order = ['shot', 'shot_02', 'shot_03', 'hit', 'destroy', 'charge', 'damage', 'dodge',
                 'slash', 'slash_02', 'slash_03', 'slash_finish', 'fever', 'skill_ready', 'clear', 'fail']
        sequence = []
        for name in order:
            sequence.extend([results[name] * 0.6, np.zeros((round(RATE * 0.24), 2))])
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        sf.write(args.preview, np.concatenate(sequence), RATE, subtype='PCM_16')


if __name__ == '__main__':
    main()
