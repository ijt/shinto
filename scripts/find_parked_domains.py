#!/usr/bin/env python3
"""Audit app/resources/top100k-domains.txt for parked/for-sale domains.

A domain-popularity dataset like Majestic Million (the source of that
list -- see its .SOURCE.md) ranks by historical/inbound-link signals, not
"does this currently serve real content" -- a chunk of any such list is
domains someone bought and never built on, sitting behind a registrar's
ad-filled "buy this domain" page. Reported concretely: an omnibox
suggestion for "x.co" led to exactly that.

This is a one-time offline maintenance script, not something Shinto runs
itself: it makes one HTTP request per domain (never repeated requests to
the same server -- the opposite of the abusive-scraping shape) and takes
a long time over 100k domains, so it's meant to be run occasionally by a
maintainer, not part of the app or its build. Writes results incrementally
(one JSON object per line) so a run can be interrupted and resumed with
--resume instead of starting over.

Usage:
    python3 scripts/find_parked_domains.py [--limit N] [--workers N]
        [--resume] [--input PATH] [--report PATH]

Then, once a report exists, prune the list with:
    python3 scripts/prune_parked_domains.py
"""
import argparse
import concurrent.futures
import json
import os
import sys
import time
from urllib.parse import urlparse

import requests

# Hosts that serve parking/marketplace pages -- if any hop in a domain's
# redirect chain lands on one of these, it's for sale/parked, full stop,
# regardless of page content (see check_domain()'s hop_urls loop). Bare
# "godaddy.com" is deliberately excluded -- too many unrelated things are
# legitimately hosted there -- but its specific parking subdomain isn't.
PARKING_REDIRECT_DOMAINS = {
    "sedoparking.com", "sedo.com", "parkingcrew.net", "bodis.com",
    "above.com", "hugedomains.com", "dan.com", "afternic.com",
    "undeveloped.com", "domainmarket.com", "namebright.com",
    "uniregistry.com", "atom.com", "trafficz.com", "smartname.com",
    "fabulous.com", "parklogic.com", "rookmedia.com", "buydomains.com",
    "domainnamesales.com", "parked.com", "voodoo.com",
    "forsale.godaddy.com",
}

# Case-insensitive substrings that show up in parked-page HTML across
# common providers -- specific phrases/hostnames, not single words, to
# keep false positives rare (an unrelated legitimate page mentioning "for
# sale" in passing shouldn't get flagged).
CONTENT_SIGNATURES = [
    "this domain is for sale", "this domain may be for sale",
    "buy this domain", "domain name has expired",
    "this web page is parked", "the domain is parked",
    "checkout the full domain details page", "backorder this domain",
    "inquire about this domain", "domain is available for purchase",
    "sedoparking.com", "parkingcrew.net", "cdn.parkingcrew",
    "bodis.com", "d38psrni17bvxu.cloudfront.net",  # Sedo's asset CDN
    "hugedomains.com", "dan.com/name/",
]

TIMEOUT = 6
USER_AGENT = "Mozilla/5.0 (compatible; ShintoParkedDomainAudit/1.0; +https://github.com/ijt/shinto)"


def _fetch(domain):
    """Returns a response, or raises the last RequestException after
    trying https then http."""
    last_error = None
    for scheme in ("https", "http"):
        try:
            return requests.get(f"{scheme}://{domain}/", timeout=TIMEOUT, allow_redirects=True,
                                 headers={"User-Agent": USER_AGENT})
        except requests.RequestException as e:
            last_error = e
    raise last_error


def check_domain(domain):
    # A domain that doesn't even resolve/connect is just as useless a
    # suggestion as a parked one (arguably more so) -- but a one-time
    # snapshot check risks mistaking a transient network blip for a truly
    # dead domain, so retry once before giving up on it.
    try:
        resp = _fetch(domain)
    except requests.RequestException:
        try:
            resp = _fetch(domain)
        except requests.RequestException as e:
            return {"domain": domain, "status": "error", "detail": str(e)[:200]}

    # Every hop, not just the final one -- a chain can pass through a
    # known parking/marketplace host (afternic.com) on its way to a final
    # host that isn't recognized, or that bot-blocks the request outright
    # (confirmed: x.co -> afternic.com -> forsale.godaddy.com, the last
    # hop returning Akamai's "Access Denied" instead of real content --
    # checking only resp.url would have missed the afternic.com hop
    # entirely).
    hop_urls = [r.url for r in resp.history] + [resp.url]
    for hop_url in hop_urls:
        host = urlparse(hop_url).hostname or ""
        for parking_domain in PARKING_REDIRECT_DOMAINS:
            if host == parking_domain or host.endswith("." + parking_domain):
                return {"domain": domain, "status": "parked", "reason": f"redirected through {host}"}

    text = resp.text[:20000].lower() if resp.text else ""
    for sig in CONTENT_SIGNATURES:
        if sig in text:
            return {"domain": domain, "status": "parked", "reason": f"content signature: {sig!r}"}

    return {"domain": domain, "status": "ok"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", default="app/resources/top100k-domains.txt")
    ap.add_argument("--report", default="scripts/parked-domain-report.jsonl")
    ap.add_argument("--limit", type=int, default=None)
    ap.add_argument("--workers", type=int, default=64)
    ap.add_argument("--resume", action="store_true")
    args = ap.parse_args()

    with open(args.input) as f:
        domains = [line.strip() for line in f if line.strip()]
    if args.limit:
        domains = domains[: args.limit]

    done = set()
    if args.resume and os.path.exists(args.report):
        with open(args.report) as f:
            for line in f:
                try:
                    done.add(json.loads(line)["domain"])
                except (json.JSONDecodeError, KeyError):
                    pass

    todo = [d for d in domains if d not in done]
    print(f"{len(domains)} total, {len(done)} already done, {len(todo)} to check", file=sys.stderr)
    if not todo:
        return

    os.makedirs(os.path.dirname(args.report) or ".", exist_ok=True)
    start = time.time()
    checked = 0
    with open(args.report, "a") as out, \
         concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as ex:
        futures = {ex.submit(check_domain, d): d for d in todo}
        for fut in concurrent.futures.as_completed(futures):
            result = fut.result()
            out.write(json.dumps(result) + "\n")
            out.flush()
            checked += 1
            if checked % 200 == 0:
                elapsed = time.time() - start
                rate = checked / elapsed
                eta_min = (len(todo) - checked) / rate / 60 if rate > 0 else 0
                print(f"{checked}/{len(todo)} checked, {rate:.1f}/s, ETA {eta_min:.1f}min",
                      file=sys.stderr)

    print("done", file=sys.stderr)


if __name__ == "__main__":
    main()
