# Photon transport experiments

Every example is self-contained. Outputs are created in the current working directory.
`examples/render-all.sh` rebuilds this gallery with lmparticle → CSV → lmplot or lmcad.

## Finite lead shields

![Finite lead shields](images/shield_geometry.png)

Four separate 1.5 × 1.5 cm shields with thicknesses 0.25, 0.5, 1 and 2 cm from left to right. Exit-beam brightness encodes computed uncollided transmission; beam shapes are schematics, not sampled photon tracks.

[Example](../examples/visual/shield_geometry.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/visual/shield_geometry.lms`

## Materials across photon energy

![Materials across photon energy](images/material_attenuation.png)

Water, aluminum and lead attenuation coefficients from the embedded XCOM data.

[Example](../examples/visual/material_attenuation.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/visual/material_attenuation.lms`

## Competing interaction processes

![Competing interaction processes](images/interaction_crossover.png)

Lead photoelectric, Compton and pair-production coefficients across energy.

[Example](../examples/visual/interaction_crossover.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/visual/interaction_crossover.lms`

## How much material halves a beam?

![How much material halves a beam?](images/half_value_layers.png)

Half-value layers at 662 keV compare equal attenuation, not equal mass or cost.

[Example](../examples/visual/half_value_layers.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/visual/half_value_layers.lms`

## Compton energy transfer

![Compton energy transfer](images/compton_transfer.png)

Seeded scattering samples compared with the analytic mean energy transfer.

[Example](../examples/visual/compton_transfer.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/visual/compton_transfer.lms`

## Scatter buildup behind lead

![Scatter buildup behind lead](images/shield_buildup.png)

Uncollided transmission follows exp(-mu x); all forward plane crossings also include scattered photons and are not a survival probability.

[Example](../examples/visual/shield_buildup.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/visual/shield_buildup.lms`

![Computed lead thickness sweep](images/shield-buildup.gif)

25 computed thickness points revealed at 8 fps, with fixed axes. `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/visual/shield_buildup.lms`

## Water removes softer photons first

![Water removes softer photons first](images/water_hardening.png)

Separate 80 and 662 keV monochromatic probes isolate preferential attenuation; lines are theory and points are Monte Carlo.

[Example](../examples/visual/water_hardening.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/visual/water_hardening.lms`

## Order matters for energy deposition

![Order matters for energy deposition](images/layer_order.png)

Reversing lead and aluminum layers preserves uncollided attenuation but changes where the beam deposits its energy. Charged particles deposit locally in this model.

[Example](../examples/visual/layer_order.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/visual/layer_order.lms`

## Original simulations

![shielding](images/shielding.png)

[Example](../examples/shielding.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/shielding.lms`

![depth dose](images/depth_dose.png)

[Example](../examples/depth_dose.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/depth_dose.lms`

![detector response](images/detector_response.png)

[Example](../examples/detector_response.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/detector_response.lms`

![pair spectrum](images/pair_spectrum.png)

[Example](../examples/pair_spectrum.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/pair_spectrum.lms`

![shielding plot](images/shielding_plot.png)

[Example](../examples/combo/shielding_plot.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/combo/shielding_plot.lms`

![detector spectrum](images/detector_spectrum.png)

[Example](../examples/combo/detector_spectrum.lms) · `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/refs/heads/main/examples/combo/detector_spectrum.lms`

The transport model uses local charged-particle energy deposition (kerma); see the [physics limits](../durable-memory/03-physics-and-limits.md).
