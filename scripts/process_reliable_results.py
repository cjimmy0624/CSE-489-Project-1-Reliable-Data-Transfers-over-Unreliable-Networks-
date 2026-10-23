#!/usr/bin/env python3
import argparse
import json
import os

import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


SCENARIOS = [
    ("loss", "loss", "Loss Rate"),
    ("delay", "delay_ms", "Delay (ms)"),
    ("reorder", "reorder", "Reorder Rate"),
    ("window", "win", "Window Size"),
]


def load_records(path):
    records = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            records.append(json.loads(line))
    return records


def format_fixed_params(records, scenario, xkey):
    keys = ["mode", "loss", "delay_ms", "reorder", "win", "timeout_ms", "file_bytes", "rate_kbps"]
    fixed = []
    for key in keys:
        if key == xkey:
            continue
        values = {rec.get(key) for rec in records if rec.get("scenario") == scenario}
        if len(values) == 1:
            val = next(iter(values))
            if val is None:
                continue
            fixed.append(f"{key}={val}")
    return ", ".join(fixed)


def plot_metric(records, scenario, xkey, xlabel, metric_key, ylabel, out_path):
    points = []
    for rec in records:
        if rec.get("scenario") != scenario:
            continue
        points.append((rec.get(xkey, 0), rec.get(metric_key, 0)))

    if not points:
        return False

    points.sort(key=lambda x: x[0])
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]

    plt.figure(figsize=(7, 4.5))
    plt.plot(xs, ys, marker="o")
    fixed = format_fixed_params(records, scenario, xkey)
    title = f"{ylabel} vs {xlabel}"
    if fixed:
        title = f"{title}\n{fixed}"
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    return True


def main():
    p = argparse.ArgumentParser(description="Process reliable JSONL results and plot")
    p.add_argument(
        "--input",
        default=os.path.join("tmp_reliable", "results.jsonl"),
        help="Path to results.jsonl",
    )
    p.add_argument(
        "--out_dir",
        default=os.path.join("tmp_reliable", "plots"),
        help="Directory for output PNGs",
    )
    p.add_argument(
        "--pdf",
        default=None,
        help="Optional PDF path for all plots (default: out_dir/reliable_plots.pdf)",
    )
    args = p.parse_args()

    records = load_records(args.input)
    if not records:
        raise SystemExit("No data found in results file.")

    os.makedirs(args.out_dir, exist_ok=True)

    outputs = []
    pdf_path = args.pdf or os.path.join(args.out_dir, "reliable_plots.pdf")
    pdf = PdfPages(pdf_path)

    for scenario, xkey, xlabel in SCENARIOS:
        goodput_path = os.path.join(args.out_dir, f"{scenario}_goodput.png")
        retx_path = os.path.join(args.out_dir, f"{scenario}_retx_rate.png")
        ok1 = plot_metric(
            records, scenario, xkey, xlabel,
            "goodput_kbps", "Goodput (kbps)", goodput_path
        )
        if ok1:
            pdf.savefig()
            plt.close()
        ok2 = plot_metric(
            records, scenario, xkey, xlabel,
            "retx_rate", "Retransmission Rate", retx_path
        )
        if ok2:
            pdf.savefig()
            plt.close()
        if ok1:
            outputs.append(goodput_path)
        if ok2:
            outputs.append(retx_path)

    pdf.close()

    if outputs:
        print("Wrote plots:")
        for path in outputs:
            print(path)
        print(f"Wrote PDF: {pdf_path}")
    else:
        print("No plots generated.")


if __name__ == "__main__":
    main()
