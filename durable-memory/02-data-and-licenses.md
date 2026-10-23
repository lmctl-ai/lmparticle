# Data and licenses

- **XCOM** (NIST Standard Reference Database 8, Berger et al.): coherent, incoherent, photoelectric,
  pair-in-nuclear-field and pair-in-electron-field mass coefficients (cm2/g) for 19 elements:
  H C N O Na Mg Al Si P S Ar K Ca Fe Cu Ge I W Pb, from 1 keV to 20 MeV on the XCOM standard grid
  (47-68 rows per element, absorption edges listed twice at the same energy: below, then above) plus
  extra energies 0.0595, 0.1224, 0.356, 0.511, 0.662, 1.022, 1.173, 1.332 MeV (Am-241, Co-57, Ba-133,
  annihilation, Cs-137, pair threshold, Co-60). 951 rows, 53 KB, embedded as `data/xcom.csv`.
- Obtained 2026-10-23 with `scripts/fetch-xcom.py`, which posts the public XCOM web form
  (`https://physics.nist.gov/cgi-bin/Xcom/xcom3_1`, needs a non-default User-Agent) per element and parses the
  result table. The tool is for maintainers; the build never touches the network.
- **License**: NIST data are U.S. Government works (17 U.S.C. 105, no copyright in the US); NIST asks that
  the database be cited, which `THIRD_PARTY_NOTICES` (embedded, `lmparticle --notices`) does. Material
  compositions come from the NIST composition tables (listed in `cpp/xs.cpp`).
- No other third-party code. The RNG (xoshiro256**, splitmix64) and Klein-Nishina sampler are re-implemented
  from published algorithms. Our code is MIT.
- Compounds are formed by the mixture rule from the element tables, exactly as XCOM does, so water,
  air, NaI etc. reproduce published XCOM compound values to the table's 4 digits (see 05-validation).
