#!/usr/bin/env python3
"""Developer tool, not part of the build: regenerate data/xcom.csv from NIST XCOM
(https://physics.nist.gov/PhysRefData/Xcom/html/xcom1.html, NIST Standard Reference Database 8,
Berger, Hubbell, Seltzer, Chang, Coursey, Sukumar, Zucker, Olsen; public data of the U.S. Department of Commerce).
Elements only: compounds and mixtures are built by the mixture rule, as XCOM itself does.
Columns (cm^2/g): coherent, incoherent, photoelectric, pair in nuclear field, pair in electron field.
Rows at an absorption edge appear twice with the same energy (below, then above)."""
import re, sys, time, urllib.parse, urllib.request, pathlib
ELEMENTS = [(1, "H"), (6, "C"), (7, "N"), (8, "O"), (11, "Na"), (12, "Mg"), (13, "Al"), (14, "Si"), (15, "P"), (16, "S"),
            (18, "Ar"), (19, "K"), (20, "Ca"), (26, "Fe"), (29, "Cu"), (32, "Ge"), (53, "I"), (74, "W"), (82, "Pb")]
# the standard XCOM grid plus common gamma lines (MeV): Am-241, annihilation, Cs-137, Co-60, Ba-133, Co-57
EXTRA = [0.0595, 0.1224, 0.356, 0.511, 0.662, 1.022, 1.173, 1.332]
out = pathlib.Path(__file__).resolve().parent.parent / "data" / "xcom.csv"
lines = ["# NIST XCOM photon cross sections, cm^2/g: Z symbol energy_MeV coherent incoherent photoelectric pair_nuclear pair_electron",
         "# Source: https://physics.nist.gov/PhysRefData/Xcom/html/xcom1.html (NIST SRD 8, public data); standard grid 1 keV-20 MeV plus extra energies"]
for z, sym in ELEMENTS:
    body = urllib.parse.urlencode({"ZNum": z, "ZSym": "", "OutOpt": "PIC", "Graph6": "on", "NumAdd": 1,
                                   "Energies": "\n".join(str(e) for e in EXTRA), "Output": "on", "WindowXmin": "0.001",
                                   "WindowXmax": "20", "ResizeFlag": "on"}).encode()
    req = urllib.request.Request("https://physics.nist.gov/cgi-bin/Xcom/xcom3_1", body, {"User-Agent": "curl/8.5.0"})
    html = urllib.request.urlopen(req, timeout=60).read().decode("latin-1")
    rows = re.findall(r'<tr BGColor="#FFFFCC">(.*?)</tr>', html, re.S)
    n = 0
    for r in rows:
        cells = [c.strip() for c in re.findall(r"<td[^>]*>(.*?)</td>", r)]
        if len(cells) != 9: sys.exit(f"Z={z}: unexpected row {cells}")
        vals = [float(c) for c in cells[1:7]]
        lines.append(f"{z} {sym} " + " ".join(f"{v:.4E}" for v in vals)); n += 1
    print(sym, n, file=sys.stderr)
    time.sleep(1)
out.write_text("\n".join(lines) + "\n")
