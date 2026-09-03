#!/usr/bin/env python3
"""Prune parked/shutdown domains from app/resources/top100k-domains.txt,
per a report produced by find_parked_domains.py.

Only removes domains flagged "parked" or "shutdown" -- positive evidence
the page itself is dead (a marketplace redirect, or a shutdown notice in
its title/URL). "error" (connection failures) is deliberately NOT
auto-pruned: some are bot-protection blocking find_parked_domains.py's
plain HTTP client rather than a genuinely dead domain (confirmed
concretely: dailyfx.com and buybuybaby.com time out for the script but
are real, working sites) -- a much weaker signal than a parking-page
redirect or a "this is down" title/URL. Errors are printed for a human
to spot-check instead of being removed automatically.

Usage:
    python3 scripts/prune_parked_domains.py [--report PATH] [--input PATH] [--apply]

Without --apply, this is a dry run: it only reports what would change.
"""
import argparse
import json


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", default="scripts/parked-domain-report.jsonl")
    ap.add_argument("--input", default="app/resources/top100k-domains.txt")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    to_remove = {}
    errors = []
    with open(args.report) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            row = json.loads(line)
            if row["status"] in ("parked", "shutdown"):
                to_remove[row["domain"]] = row
            elif row["status"] == "error":
                errors.append(row)

    with open(args.input) as f:
        domains = [line.rstrip("\n") for line in f]

    kept = [d for d in domains if d.strip() not in to_remove]
    removed_count = len(domains) - len(kept)

    print(f"{len(domains)} domains in list")
    print(f"{removed_count} to remove (parked/shutdown)")
    print(f"{len(errors)} connection errors -- NOT auto-removed, review manually "
          "(some are likely bot-protected, not dead)")
    print()
    print("=== removed ===")
    for domain, row in sorted(to_remove.items()):
        print(f"  {domain}: {row['status']} -- {row['reason']}")

    if errors:
        print()
        print("=== connection errors (for manual review) ===")
        for row in sorted(errors, key=lambda r: r["domain"]):
            print(f"  {row['domain']}: {row['detail']}")

    if args.apply:
        with open(args.input, "w") as f:
            f.write("\n".join(kept) + "\n")
        print(f"\nWrote {len(kept)} domains to {args.input}")
    else:
        print("\n(dry run -- pass --apply to actually write the pruned list)")


if __name__ == "__main__":
    main()
