#!/usr/bin/env python3

import csv
import html
import sys
from pathlib import Path


SCALE = 10_000_000
COLORS = ["#2563eb", "#d97706", "#059669", "#dc2626", "#7c3aed"]


def load_waveforms(path):
    with path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    times = [int(row["time_from_pacemaker_q1_ms"]) for row in rows]
    values = [
        [int(row[f"cell_{cell}_egm_scaled"]) / SCALE for row in rows]
        for cell in range(1, 6)
    ]
    return times, values


def load_q1(path):
    with path.open(newline="", encoding="utf-8") as source:
        return {
            int(row["cell"]): int(row["time_from_pacemaker_q1_ms"])
            for row in csv.DictReader(source)
        }


def main():
    if len(sys.argv) != 5:
        raise SystemExit("usage: render_egm_waveforms.py WAVEFORMS.csv Q1.csv OUTPUT.svg TIMESTEP_MS")

    waveform_path = Path(sys.argv[1])
    q1_path = Path(sys.argv[2])
    output_path = Path(sys.argv[3])
    timestep_ms = int(sys.argv[4])
    times, values = load_waveforms(waveform_path)
    q1_times = load_q1(q1_path)

    width, height = 1100, 900
    left, right, top, panel_h, gap = 92, 28, 72, 142.8, 14
    plot_w = width - left - right
    x_min, x_max = min(times), max(times)
    y_min, y_max = -8.5, 6.5

    def x(value):
        return left + (value - x_min) * plot_w / (x_max - x_min)

    def y(panel, value):
        panel_top = top + panel * (panel_h + gap)
        return panel_top + (y_max - value) * panel_h / (y_max - y_min)

    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">',
        f'<title id="title">Natural five-cell EGM waveforms at {timestep_ms} milliseconds</title>',
        '<desc id="desc">Verilator EGM waveforms for the naturally generated Cell 1 pacemaker cycle, including baseline before Q1.</desc>',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        '<style>text{font-family:system-ui,-apple-system,Segoe UI,sans-serif;fill:#172033}.title{font-size:24px;font-weight:600}.sub{font-size:14px;fill:#526077}.cell{font-size:15px;font-weight:600}.tick{font-size:12px;fill:#526077}.grid{stroke:#d9dee7;stroke-width:1}.zero{stroke:#8490a3;stroke-width:1.2}.q1{stroke:#526077;stroke-width:1;stroke-dasharray:5 4}.wave{fill:none;stroke-width:2.2;stroke-linejoin:round;stroke-linecap:round}</style>',
        f'<text class="title" x="{left}" y="32">Natural five-cell EGM waveforms — {timestep_ms} ms timestep</text>',
        f'<text class="sub" x="{left}" y="54">Intrinsic intervals: 20, 23, 26, 30, 40 s · no forced state · integer scale {SCALE:,}</text>',
    ]

    for panel in range(5):
        panel_top = top + panel * (panel_h + gap)
        out.append(f'<rect x="{left}" y="{panel_top:.2f}" width="{plot_w}" height="{panel_h}" fill="#fbfcfe" stroke="#c9d0dc"/>')
        for tick in (-8, -6, -4, -2, 0, 2, 4, 6):
            yy = y(panel, tick)
            klass = "zero" if tick == 0 else "grid"
            out.append(f'<line class="{klass}" x1="{left}" x2="{left + plot_w}" y1="{yy:.2f}" y2="{yy:.2f}"/>')
            out.append(f'<text class="tick" x="{left - 10}" y="{yy + 4:.2f}" text-anchor="end">{tick}</text>')

        q1 = q1_times[panel + 1]
        qx = x(q1)
        out.append(f'<line class="q1" x1="{qx:.2f}" x2="{qx:.2f}" y1="{panel_top:.2f}" y2="{panel_top + panel_h:.2f}"/>')
        out.append(f'<text class="tick" x="{qx + 5:.2f}" y="{panel_top + 14:.2f}">Q1</text>')
        points = " ".join(
            f"{x(time):.2f},{y(panel, value):.2f}"
            for time, value in zip(times, values[panel])
        )
        out.append(f'<polyline class="wave" stroke="{COLORS[panel]}" points="{html.escape(points)}"/>')
        out.append(f'<text class="cell" x="18" y="{panel_top + 22:.2f}" fill="{COLORS[panel]}">Cell {panel + 1}</text>')

    bottom = top + 4 * (panel_h + gap) + panel_h
    for tick in range(-1000, 4501, 500):
        xx = x(tick)
        out.append(f'<line stroke="#8490a3" x1="{xx:.2f}" x2="{xx:.2f}" y1="{bottom:.2f}" y2="{bottom + 5:.2f}"/>')
        out.append(f'<text class="tick" x="{xx:.2f}" y="{bottom + 20:.2f}" text-anchor="middle">{tick}</text>')
    out.append('<text class="tick" transform="translate(20 450) rotate(-90)" text-anchor="middle">EGM potential (model units)</text>')
    out.append(f'<text class="tick" x="{left + plot_w / 2:.2f}" y="888" text-anchor="middle">Time relative to natural Cell 1 Q1 (ms)</text>')
    out.append(f'<text class="sub" x="{left + plot_w}" y="54" text-anchor="end">One-second pre-Q1 baseline shown</text>')
    out.append('</svg>')
    output_path.write_text("\n".join(out) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
