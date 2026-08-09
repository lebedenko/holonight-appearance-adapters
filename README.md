# HoloNight Appearance Adapters

Experimental, non-installed consumers and projections for HoloNight's semantic
appearance contract. CTV-005 validates the wire format and GTK Tier 1 mechanisms;
production mutation and installation remain gated on CTV-007.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

See [the CTV-005 SDD](docs/sdd/ctv-005-semantic-consumer/SPEC.md) for the
contract, evidence, applicability matrix, and handoffs.
