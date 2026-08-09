# HoloNight Appearance Adapters

Experimental, non-installed consumers and projections for HoloNight's semantic
appearance contract. CTV-005 validates the wire format and GTK Tier 1 mechanisms;
CTV-006 evaluates separate GTK 3 and GTK 4 Tier 2 palette fragments. Production
mutation and installation remain gated on CTV-007.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

See [the CTV-005 SDD](docs/sdd/ctv-005-semantic-consumer/SPEC.md) for the
contract, evidence, applicability matrix, and handoffs.

See [the CTV-006 SDD](docs/sdd/ctv-006-gtk-palette/SPEC.md) for the independent
GTK-major mappings, compatibility evidence, limitations, and recommendations.
